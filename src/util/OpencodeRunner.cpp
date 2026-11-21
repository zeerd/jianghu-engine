/**
 * @file OpencodeRunner.cpp
 * @brief opencode CLI 分析实现：命令行拼装、JSONL 输出解析与进程执行。 */

// windows.h 必须最先包含：进程执行段用到 CreatePipe/ReadFile 等句柄 API，
// 后置引入会先被其它头的宏改动而漏掉部分声明
#include <windows.h>

#include "OpencodeRunner.h"

#include "Logger.h"
#include "StringUtil.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>

// JSONL 事件流解析用 xpjson。它在 _WIN32 下 #define snprintf _snprintf，
// 用完即关，避免宏外泄到本文件后续代码。
#include "xpjson.hpp"
#undef snprintf

// ---------------------------------------------------------------------
// 命令行拼装
// ---------------------------------------------------------------------

// Windows 命令行参数引用：含空白或引号时整体加引号；引号前与结尾的
// 反斜杠成对加倍，保证 CRT 参数解析还原出原参数。
static std::string quote_arg(const std::string& arg)
{
    if (arg.empty()) return "\"\"";
    if (arg.find_first_of(" \t\"") == std::string::npos) return arg;

    std::string out;
    out.reserve(arg.size() + 2);
    out += '"';
    size_t backslashes = 0;
    for (char c : arg) {
        if (c == '\\') {
            ++backslashes;
            continue;
        }
        if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out += '"';
            backslashes = 0;
            continue;
        }
        out.append(backslashes, '\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, '\\');
    out += '"';
    return out;
}

// ---------------------------------------------------------------------
// JSONL 输出解析
// ---------------------------------------------------------------------

bool parse_opencode_output(const std::string& jsonl, std::string& out_text,
                           std::string& out_error)
{
    out_text.clear();
    out_error.clear();

    size_t pos = 0;
    while (pos < jsonl.size()) {
        size_t eol = jsonl.find('\n', pos);
        if (eol == std::string::npos) eol = jsonl.size();
        std::string line = jsonl.substr(pos, eol - pos);
        pos              = eol + 1;

        // 裁掉行尾 \r 与空白；空行与非事件行（诊断输出）直接放过
        while (!line.empty()
               && (line.back() == '\r' || line.back() == ' '
                   || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty() || line.front() != '{') continue;

        JSON::Value v;
        try {
            v.read(line.data(), line.size());
        } catch (const std::exception&) {
            continue;
        }
        if (v.type() != JSON::OBJECT) continue;
        const JSON::Object& obj = v.o();

        auto type_it = obj.find("type");
        if (type_it == obj.end() || type_it->second.type() != JSON::STRING) {
            continue;
        }
        const std::string& type = type_it->second.s();

        if (type == "text") {
            // 助手文本分片在 part.text，多片依序拼接
            auto part_it = obj.find("part");
            if (part_it == obj.end()
                || part_it->second.type() != JSON::OBJECT) {
                continue;
            }
            const JSON::Object& part = part_it->second.o();
            auto text_it             = part.find("text");
            if (text_it == part.end()
                || text_it->second.type() != JSON::STRING) {
                continue;
            }
            if (!out_text.empty()) out_text += '\n';
            out_text += text_it->second.s();
        }
        else if (type == "error") {
            // 错误事件（配额、鉴权、模型不可用）：记原因，交调用方报失败
            auto err_it = obj.find("error");
            if (err_it == obj.end() || err_it->second.type() != JSON::OBJECT) {
                continue;
            }
            const JSON::Object& err = err_it->second.o();
            auto msg_it             = err.find("message");
            if (msg_it == err.end() || msg_it->second.type() != JSON::STRING) {
                continue;
            }
            if (!out_error.empty()) out_error += "; ";
            out_error += msg_it->second.s();
        }
    }
    return !out_text.empty();
}

// ---------------------------------------------------------------------
// COpencodeRunner
// ---------------------------------------------------------------------

COpencodeRunner::COpencodeRunner(std::unique_ptr<IProcessRunner> process,
                                 std::string cli_path, std::string work_dir,
                                 std::string model, int timeout_sec)
    : m_process(std::move(process)),
      m_cli_path(std::move(cli_path)),
      m_work_dir(std::move(work_dir)),
      m_model(std::move(model)),
      m_timeout_sec(timeout_sec)
{
}

bool COpencodeRunner::analyze(const std::string& prompt,
                              const std::string& context_file,
                              std::string& out_plan)
{
    out_plan.clear();
    if (!m_process || m_cli_path.empty()) return false;

    // 非交互分析：事件流按 JSON 输出，日志上下文走 --file 附件，
    // 提示词不含日志——命令行长度不随日志规模增长。
    // 跨进程的路径必须自明：opencode 在 work_dir 下运行，相对路径会按
    // 它的工作目录去找——兜底解析为绝对路径（调用方本应给出绝对路径）。
    std::string args = "run --format json --auto";
    if (!m_model.empty()) args += " --model " + quote_arg(m_model);
    args += " --file " + quote_arg(absolute_path(context_file));
    args += " " + quote_arg(prompt);

    std::string output;
    int timeout_ms = m_timeout_sec > 0 ? m_timeout_sec * 1000 : 0;
    const ProcessResult result
        = m_process->run(m_cli_path, args, m_work_dir, timeout_ms, output);
    if (result == ProcessResult::TimedOut) {
        LOG_INFO("opencode: analysis timed out after %ds (terminated)",
                 m_timeout_sec);
        return false;
    }
    if (result == ProcessResult::Canceled) {
        LOG_INFO("opencode: analysis canceled");
        return false;
    }
    if (result != ProcessResult::Ok) {
        LOG_INFO("opencode: process failed to run (exe=%s)",
                 m_cli_path.c_str());
        return false;
    }

    std::string err;
    if (!parse_opencode_output(output, out_plan, err)) {
        LOG_INFO("opencode: no assistant text (%s)",
                 err.empty() ? "empty output" : err.c_str());
        out_plan.clear();
        return false;
    }
    return true;
}

void COpencodeRunner::cancel()
{
    if (m_process) m_process->cancel();
}

// ---------------------------------------------------------------------
// CWinProcessRunner
// ---------------------------------------------------------------------

// 排空管道当前可读数据（Peek 驱动、不阻塞）：写端全关且缓冲读空后
// Peek 失败退出；若别处仍持有写句柄则读空即退，不纠缠收尾。
static void drain_pipe(HANDLE pipe, std::string& out)
{
    DWORD avail = 0;
    while (PeekNamedPipe(pipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
        char buf[4096];
        DWORD to_read = avail < sizeof(buf) ? avail : (DWORD)sizeof(buf);
        DWORD got     = 0;
        if (!ReadFile(pipe, buf, to_read, &got, NULL) || got == 0) break;
        out.append(buf, got);
    }
}

ProcessResult CWinProcessRunner::run(const std::string& exe,
                                     const std::string& args,
                                     const std::string& cwd, int timeout_ms,
                                     std::string& out_output)
{
    out_output.clear();
    if (m_cancel.load()) return ProcessResult::Canceled;
    if (exe.empty()) return ProcessResult::Failed;

    // 匿名管道承接 stdout/stderr 合流：写端可继承给子进程，读端留在父进程
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength             = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle      = TRUE;
    HANDLE read_pipe       = NULL;
    HANDLE write_pipe      = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return ProcessResult::Failed;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    // stdin 接 NUL：opencode run 不读标准输入，但子进程需要一个有效句柄
    HANDLE nul_in
        = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOEXA siex         = {0};
    siex.StartupInfo.cb         = sizeof(STARTUPINFOEXA);
    siex.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    siex.StartupInfo.hStdInput  = nul_in;
    siex.StartupInfo.hStdOutput = write_pipe;
    siex.StartupInfo.hStdError  = write_pipe;

    // 句柄白名单：只把管道两端交给子进程。全量继承（bInheritHandles）
    // 会把本进程其它可继承句柄（标准输出管道、套接字）一并塞给子进程——
    // 子进程或其残留后代攥着这些句柄不放，父进程的管道就永不 EOF。
    HANDLE inherit_list[2] = {0};
    DWORD inherit_count    = 0;
    if (nul_in != INVALID_HANDLE_VALUE) inherit_list[inherit_count++] = nul_in;
    inherit_list[inherit_count++] = write_pipe;

    SIZE_T attr_bytes = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attr_bytes);
    auto* attrs = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attr_bytes));
    bool attrs_ok
        = attrs != NULL
          && InitializeProcThreadAttributeList(attrs, 1, 0, &attr_bytes)
          && UpdateProcThreadAttribute(
              attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_list,
              inherit_count * sizeof(HANDLE), NULL, NULL);
    if (!attrs_ok) {
        if (attrs) {
            DeleteProcThreadAttributeList(attrs);
            free(attrs);
        }
        CloseHandle(write_pipe);
        if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
        CloseHandle(read_pipe);
        LOG_INFO("log_monitor: process attribute setup failed");
        return ProcessResult::Failed;
    }

    PROCESS_INFORMATION pi = {0};

    // 作业对象：本进程启动的子树必须随 run() 返回收干净。超时/取消只能
    // 终止直接子进程，漏网的孙进程（直接子进程再拉起的）会攥着继承句柄
    // 不放，父进程的管道与外部读端就永不 EOF。
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
        limits.BasicLimitInformation.LimitFlags
            = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                     &limits, sizeof(limits))) {
            CloseHandle(job);
            job = NULL;
        }
    }

    // exe 由配置给定、不含引号；args 已按 quote_arg 规则拼好
    std::string cmdline = "\"" + exe + "\" " + args;
    BOOL created        = CreateProcessA(
        NULL, &cmdline[0], NULL, NULL, TRUE,
        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, NULL,
        cwd.empty() ? NULL : cwd.c_str(), &siex.StartupInfo, &pi);

    DeleteProcThreadAttributeList(attrs);
    free(attrs);

    // 父进程立刻交出自己的写端与 NUL：子进程退出后管道即到 EOF
    CloseHandle(write_pipe);
    if (nul_in != INVALID_HANDLE_VALUE) CloseHandle(nul_in);
    if (!created) {
        CloseHandle(read_pipe);
        if (job) CloseHandle(job);
        return ProcessResult::Failed;
    }
    if (job && !AssignProcessToJobObject(job, pi.hProcess)) {
        // 赶不上挂载（子进程秒退）或处于不允许嵌套的旧作业：
        // 放弃配树回收，退回「只终止直接子进程」的语义
        CloseHandle(job);
        job = NULL;
    }

    // 轮询等待：排空与超时/取消判定同拍进行——不引入读线程，避免
    // 「别处还持有写句柄」时 join 读线程把监控线程挂死。
    std::string captured;
    bool finished     = false;
    int elapsed_ms    = 0;
    const int kPollMs = 50;
    while (true) {
        DWORD rc = WaitForSingleObject(pi.hProcess, kPollMs);
        drain_pipe(read_pipe, captured);
        if (rc == WAIT_OBJECT_0) {
            finished = true;
            break;
        }
        if (rc == WAIT_FAILED) break;
        if (m_cancel.load()) break;
        if (timeout_ms > 0) {
            elapsed_ms += kPollMs;
            if (elapsed_ms >= timeout_ms) break;
        }
    }

    if (!finished) {
        // 自有子进程：启动即持有句柄，超时/取消按句柄终止（进程安全规则）
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 2000);
        drain_pipe(read_pipe, captured);  // 终止前已写出的尾部照收
    }

    CloseHandle(read_pipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    // 关闭作业句柄即回收整棵子树：超时路径漏下的后代到此收干净
    if (job) CloseHandle(job);

    if (finished) {
        out_output = std::move(captured);
        return ProcessResult::Ok;
    }
    // 未跑完：分清是谁打断的——超时、取消、等待句柄故障语义各不同
    if (m_cancel.load()) return ProcessResult::Canceled;
    if (timeout_ms > 0 && elapsed_ms >= timeout_ms) {
        return ProcessResult::TimedOut;
    }
    return ProcessResult::Failed;
}

void CWinProcessRunner::cancel() { m_cancel.store(true); }

/**
 * @file OpencodeRunner.h
 * @brief 调用 opencode CLI 分析日志：进程执行抽象与 JSONL 输出解析。
 *
 * 分层：日志监控只依赖 IOpencodeRunner；命令行拼装与输出解析在
 * COpencodeRunner；进程创建/超时/取消下沉到 IProcessRunner——进程机制
 * （Win32 下为 CreateProcess）属平台细节，单独替换不影响分析语义。
 */

#pragma once

#include <atomic>
#include <memory>
#include <string>

// 进程执行结果：失败模式分流——超时、取消与进程故障的处置和
// 留痕语义各不相同（超时=分析太慢、已连子进程树一起终止；取消=关机
// 主动打断；故障=没跑起来）。
enum class ProcessResult {
    Ok,        // 进程正常跑完
    Failed,    // 启动失败、等待句柄失败或异常退出
    TimedOut,  // 超时，子进程（树）已终止
    Canceled,  // 被 cancel() 打断，子进程（树）已终止
};

// 外部进程执行：opencode CLI 的进程层。
class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;

    // 执行 exe（完整命令行 args 已拼好），cwd 为空时继承当前目录。
    // stdout 与 stderr 合流捕获进 out_output。timeout_ms <= 0 表示不限时。
    // 超时或 cancel() 后终止自有子进程（含其后代）。
    virtual ProcessResult run(const std::string& exe, const std::string& args,
                              const std::string& cwd, int timeout_ms,
                              std::string& out_output) = 0;

    // 请求取消：在跑的子进程被终止，run() 尽快返回 Canceled。
    virtual void cancel() = 0;
};

// opencode 分析入口：读上下文文件，产出 plan 文本。
class IOpencodeRunner {
public:
    virtual ~IOpencodeRunner() = default;

    // 以 prompt 分析 context_file 中的日志片段；成功时 out_plan 为助手
    // 输出的 plan。失败（进程未跑完 / 无助手文本）返回 false，原因记 INFO。
    virtual bool analyze(const std::string& prompt,
                         const std::string& context_file, std::string& out_plan)
        = 0;

    // 取消在途分析（关机路径：停监控时立刻归还监控线程）
    virtual void cancel() = 0;
};

// 从 opencode run --format json 的 JSONL 输出提取助手文本：
// type=="text" 事件的 part.text 依序拼接；type=="error" 事件的
// error.message 记入 out_error。返回是否提取到助手文本。
bool parse_opencode_output(const std::string& jsonl, std::string& out_text,
                           std::string& out_error);

// 分析实现：拼命令行 → 进程执行 → JSONL 解析。
// model 为空时交给 opencode 的默认模型。
class COpencodeRunner : public IOpencodeRunner {
public:
    COpencodeRunner(std::unique_ptr<IProcessRunner> process,
                    std::string cli_path, std::string work_dir,
                    std::string model, int timeout_sec);

    bool analyze(const std::string& prompt, const std::string& context_file,
                 std::string& out_plan) override;

    void cancel() override;

private:
    std::unique_ptr<IProcessRunner> m_process;
    std::string m_cli_path;
    std::string m_work_dir;
    std::string m_model;
    int m_timeout_sec;
};

// CreateProcess 实现：匿名管道捕获输出，50ms 切片轮询等待，
// 超时/取消按自身句柄终止子进程（进程安全：只动自己启动的进程）。
class CWinProcessRunner : public IProcessRunner {
public:
    ProcessResult run(const std::string& exe, const std::string& args,
                      const std::string& cwd, int timeout_ms,
                      std::string& out_output) override;

    void cancel() override;

private:
    std::atomic<bool> m_cancel{false};
};

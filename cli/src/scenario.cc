#include "scenario_runner.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

#include "client_core.h"
#include "game_state.h"

namespace {

// repeat 区块展开的上限：防止误写超大次数把步骤表撑爆。
constexpr long long MAX_REPEAT_COUNT = 10000;

using Step               = CScenarioRunner::Step;
using RepeatFrame        = CScenarioRunner::RepeatFrame;

// 生成「第 index 轮迭代把序号写入 var」的 set 步骤。
Step make_index_step(const RepeatFrame &frame, int index)
{
    Step step;
    step.verb    = "set";
    step.args    = frame.var + " " + std::to_string(index + 1);
    step.line_no = frame.line_no;
    return step;
}

}  // namespace

std::string CScenarioRunner::trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string CScenarioRunner::lower_ascii(std::string s)
{
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool CScenarioRunner::all_digits(const std::string &s)
{
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

CScenarioRunner::CScenarioRunner(ClientCore &core, const ScenarioOptions &opts)
    : core_(core), opts_(opts)
{
}

bool CScenarioRunner::load_vars()
{
    if (opts_.vars_path.empty()) return true;
    std::ifstream in(opts_.vars_path);
    if (!in) return true;  // 首次运行没有变量文件是正常情况
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        vars_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return true;
}

void CScenarioRunner::save_vars() const
{
    if (opts_.vars_path.empty()) return;
    std::ofstream out(opts_.vars_path, std::ios::trunc);
    if (!out) return;
    for (const auto &[k, v] : vars_) out << k << "=" << v << "\n";
}

std::string CScenarioRunner::subst(const std::string &in) const
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        if (in[i] == '$') {
            if (i + 1 < in.size() && in[i + 1] == '{') {
                size_t end = in.find('}', i + 2);
                if (end != std::string::npos) {
                    std::string name = in.substr(i + 2, end - i - 2);
                    auto it          = vars_.find(name);
                    if (it != vars_.end()) {
                        out += it->second;
                        i = end + 1;
                        continue;
                    }
                }
            }
            else if (i + 1 < in.size()
                     && (std::isalpha(static_cast<unsigned char>(in[i + 1]))
                         || in[i + 1] == '_')) {
                size_t j = i + 1;
                while (j < in.size()
                       && (std::isalnum(static_cast<unsigned char>(in[j]))
                           || in[j] == '_')) {
                    ++j;
                }
                std::string name = in.substr(i + 1, j - i - 1);
                auto it          = vars_.find(name);
                if (it != vars_.end()) {
                    out += it->second;
                    i = j;
                    continue;
                }
            }
        }
        out += in[i++];
    }
    return out;
}

bool CScenarioRunner::load_script(const std::string &path, int depth)
{
    if (depth > 8) {
        fail_reason_ = "include nesting too deep: " + path;
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fail_reason_ = "cannot open script: " + path;
        return false;
    }

    std::string base_dir = path;
    size_t slash         = base_dir.find_last_of("\\/");
    base_dir             = (slash == std::string::npos)
                               ? std::string()
                               : base_dir.substr(0, slash + 1);

    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (line_no == 1 && line.size() >= 3
            && static_cast<unsigned char>(line[0]) == 0xEF
            && static_cast<unsigned char>(line[1]) == 0xBB
            && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);  // UTF-8 BOM
        }
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t sp = line.find_first_of(" \t");
        Step step;
        step.verb    = (sp == std::string::npos) ? line : line.substr(0, sp);
        step.args    = (sp == std::string::npos) ? "" : trim(line.substr(sp + 1));
        step.line_no = line_no;
        step.verb    = lower_ascii(step.verb);

        if (step.verb == "include") {
            std::string inc = subst(step.args);
            if (!inc.empty() && (inc[0] == '/' || inc[0] == '\\'
                                 || (inc.size() > 1 && inc[1] == ':'))) {
                // 绝对路径
            }
            else {
                inc = base_dir + inc;
            }
            if (!load_script(inc, depth + 1)) return false;
            continue;
        }

        steps_.push_back(std::move(step));
    }
    return true;
}

bool CScenarioRunner::parse_repeat(const Step &step, RepeatFrame &frame)
{
    std::istringstream iss(subst(step.args));
    std::string count_text;
    if (!(iss >> count_text) || !all_digits(count_text)) {
        return fail("line " + std::to_string(step.line_no)
                    + ": repeat needs: repeat <count> [as <var>]");
    }
    long long count = std::stoll(count_text);
    if (count > MAX_REPEAT_COUNT) {
        return fail("line " + std::to_string(step.line_no)
                    + ": repeat count exceeds " + std::to_string(MAX_REPEAT_COUNT));
    }
    frame.count   = static_cast<int>(count);
    frame.line_no = step.line_no;

    std::string as;
    if (iss >> as) {
        if (as != "as" || !(iss >> frame.var)) {
            return fail("line " + std::to_string(step.line_no)
                        + ": repeat needs: repeat <count> [as <var>]");
        }
    }
    return true;
}

bool CScenarioRunner::expand_loops()
{
    std::vector<Step> out;
    if (!expand_range(steps_, 0, steps_.size(), 0, out)) return false;
    steps_ = std::move(out);
    return true;
}

bool CScenarioRunner::expand_range(const std::vector<Step> &src, size_t begin,
                                   size_t end, int depth,
                                   std::vector<Step> &out)
{
    if (depth > 8) {
        return fail("repeat nesting too deep");
    }
    for (size_t i = begin; i < end; ++i) {
        const Step &step = src[i];
        if (step.verb == "end") {
            return fail("line " + std::to_string(step.line_no)
                        + ": end without repeat");
        }
        if (step.verb != "repeat") {
            out.push_back(step);
            continue;
        }

        RepeatFrame frame;
        if (!parse_repeat(step, frame)) return false;

        size_t close = i + 1;
        int nest     = 0;
        for (; close < end; ++close) {
            if (src[close].verb == "repeat") {
                ++nest;
            }
            else if (src[close].verb == "end") {
                if (nest == 0) break;
                --nest;
            }
        }
        if (close == end) {
            return fail("line " + std::to_string(step.line_no)
                        + ": repeat without end");
        }

        std::vector<Step> body;
        if (!expand_range(src, i + 1, close, depth + 1, body)) return false;
        for (int n = 0; n < frame.count; ++n) {
            if (!frame.var.empty()) out.push_back(make_index_step(frame, n));
            out.insert(out.end(), body.begin(), body.end());
        }
        i = close;
    }
    return true;
}

bool CScenarioRunner::fail(const std::string &reason)
{
    fail_reason_ = reason;
    return false;
}

void CScenarioRunner::echo_step(const Step &step) const
{
    if (!opts_.echo) return;
    std::cout << "[script:" << step.line_no << "] " << step.verb;
    if (!step.args.empty()) std::cout << " " << step.args;
    // 无头运行的 stdout 是被 E2E 脚本重定向的管道（全缓冲），
    // 不 flush 就看不到逐步进度，整段剧本会长时间毫无反馈。
    std::cout << std::endl;
}

bool CScenarioRunner::write_report(const std::string &status) const
{
    if (opts_.report_path.empty()) return true;
    std::ofstream out(opts_.report_path, std::ios::trunc);
    if (!out) return false;
    out << "status=" << status << "\n";
    if (!fail_reason_.empty()) out << "reason=" << fail_reason_ << "\n";
    if (fail_line_ > 0) out << "line=" << fail_line_ << "\n";
    for (const auto &[k, v] : vars_) out << "var." << k << "=" << v << "\n";
    return true;
}

int CScenarioRunner::run()
{
    load_vars();
    if (!load_script(opts_.script_path) || !expand_loops()) {
        std::cerr << "[scenario] " << fail_reason_ << "\n";
        write_report("fail");
        return 2;
    }

    if (opts_.echo) {
        std::cout << "[scenario] running " << opts_.script_path << " ("
                  << steps_.size() << " steps)" << std::endl;
    }

    for (const auto &step : steps_) {
        if (core_.server_closed()) {
            fail_line_ = step.line_no;
            fail_reason_ = "server closed the connection";
            std::cerr << "[scenario] FAIL at line " << step.line_no << ": "
                      << fail_reason_ << "\n";
            write_report("fail");
            return 1;
        }
        if (!run_step(step)) {
            fail_line_ = step.line_no;
            std::cerr << "[scenario] FAIL at line " << step.line_no << ": "
                      << fail_reason_ << "\n";
            write_report("fail");
            return 1;
        }
    }

    std::cout << "[scenario] PASS: " << steps_.size() << " steps"
              << std::endl;
    write_report("pass");
    return 0;
}

int run_scenario(ClientCore &core, const ScenarioOptions &opts)
{
    CScenarioRunner runner(core, opts);
    return runner.run();
}

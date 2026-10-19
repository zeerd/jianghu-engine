#pragma once

#include <map>
#include <string>
#include <vector>

#include "scenario.h"

class ClientCore;

/// 无头剧本运行器：把 .sce 装载成步骤序列（include 内联、repeat/end 区块
/// 按次数展开），再逐步执行、回显并写报告。
class CScenarioRunner {
public:
    CScenarioRunner(ClientCore &core, const ScenarioOptions &opts);

    /// 运行剧本。0=全部通过，1=步骤执行失败，2=剧本装载失败。
    int run();

    /// 单条剧本步骤。
    struct Step {
        std::string verb;
        std::string args;
        int line_no = 0;
    };

    /// repeat 指令在区块展开期间的状态。
    struct RepeatFrame {
        int count = 0;    // 重复次数
        std::string var;  // 可选：每轮迭代写入的序号变量名
        int line_no = 0;  // repeat 指令自身行号（错误报告用）
    };

protected:
    bool load_vars();
    void save_vars() const;
    bool load_script(const std::string &path, int depth = 0);
    bool parse_repeat(const Step &step, RepeatFrame &frame);
    bool expand_loops();
    bool expand_range(const std::vector<Step> &src, size_t begin, size_t end,
                      int depth, std::vector<Step> &out);

    std::string subst(const std::string &in) const;
    bool eval_cond(const std::string &cond, std::string &detail);
    bool eval_attr_cond(const std::string &val, std::string &detail);
    bool wait_cond(const std::string &cond, int timeout_ms);
    bool run_step(const Step &step);
    bool run_menu_step(const std::string &want);
    bool run_capture_step(const std::string &args);

    bool fail(const std::string &reason);
    void echo_step(const Step &step) const;
    bool write_report(const std::string &status) const;

    static std::string trim(const std::string &s);
    static std::string lower_ascii(std::string s);
    static bool all_digits(const std::string &s);

    ClientCore &core_;
    ScenarioOptions opts_;
    std::map<std::string, std::string> vars_;
    std::vector<Step> steps_;
    std::string fail_reason_;
    int fail_line_ = 0;
};

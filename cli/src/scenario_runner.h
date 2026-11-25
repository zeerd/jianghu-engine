#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "scenario.h"

class ClientCore;

/// 无头剧本运行器：把 .sce 装载成步骤序列（include 内联、repeat/end 区块
/// 按次数展开），再逐步执行、回显并写报告。
class CScenarioRunner {
public:
    CScenarioRunner(ClientCore& core, const ScenarioOptions& opts);

    /// 运行剧本。0=全部通过，1=步骤执行失败，2=剧本装载失败。
    int run();

    /// 单条剧本步骤。
    struct Step {
        std::string verb;
        std::string args;
        int line_no = 0;
        // 来源剧本（相对顶层剧本目录、'/' 分隔）。include 内联后行号只在
        // 单文件内唯一，归属剧本是分场景计时与失败定位的唯一依据。
        std::string scenario;
    };

    /// repeat 指令在区块展开期间的状态。
    struct RepeatFrame {
        int count = 0;         // 重复次数
        std::string var;       // 可选：每轮迭代写入的序号变量名
        int line_no = 0;       // repeat 指令自身行号（错误报告用）
        std::string scenario;  // repeat 所在剧本（序号步骤归属它）
    };

protected:
    bool load_vars();
    void save_vars() const;
    // scenario：本文件的归属名（顶层为文件名，include 为相对 script_root_ 的
    // 路径）；分场景计时按它把步骤归堆。
    bool load_script(const std::string& path, int depth = 0,
                     const std::string& scenario = std::string());
    bool parse_repeat(const Step& step, RepeatFrame& frame);
    bool expand_loops();
    bool expand_range(const std::vector<Step>& src, size_t begin, size_t end,
                      int depth, std::vector<Step>& out);

    std::string subst(const std::string& in) const;

    /// `text` 条件的命中位置。`consume` 语义要用：命中行随后被消费游标
    /// 丢出缓冲，行文本得先存下来给紧跟着的 `capture` 读。
    struct TextHit {
        bool valid = false;
        size_t abs = 0;    // 绝对行号
        std::string line;  // 该行全文
    };

    bool eval_cond(const std::string& cond, std::string& detail,
                   TextHit* hit = nullptr);
    bool eval_attr_cond(const std::string& val, std::string& detail);
    // 取单个属性的当前值（未知名返回 false）。eval_attr_cond 与超时诊断共用
    // 同一份属性名单，避免两处各写一份而漂移。
    bool attr_value(const std::string& name, long long& out) const;
    // 把 `attr <name> ...` 的实际值拼成 "<name>=<当前值>"，供超时失败原因附带。
    std::string attr_snapshot(const std::string& cond) const;
    // consume：命中后把消费游标推到命中行之后，供 `wait text … consume`
    // 在循环里按到达顺序逐条推进（详见 scenario_steps.cc 的实现注释）。
    bool wait_cond(const std::string& cond, int timeout_ms,
                   bool consume = false);
    bool run_step(const Step& step);
    bool run_menu_step(const std::string& want);
    bool run_capture_step(const std::string& args);
    // 从一段文本里摘掉独立的空白分隔 token（如 `consume`），命中返回 true。
    // 只摘整词，不动其余字节——条件里的 needle 可能含空格。
    static bool strip_token(std::string& s, const std::string& tok);

    bool fail(const std::string& reason);
    void echo_step(const Step& step) const;
    bool write_report(const std::string& status) const;

    // 分场景计时：回显带 [+-]秒 前缀，步骤跨剧本时打 [timing] 边界，
    // 收口打 [timing] 汇总——报告侧据此定位「哪段剧本吃掉了墙钟」。
    double elapsed_sec() const;
    std::string scenario_name_of(const std::string& path) const;
    void open_scenario(const std::string& scenario);
    void close_scenario(double now_sec);
    void print_timing() const;

    static std::string trim(const std::string& s);
    static std::string lower_ascii(std::string s);
    static bool all_digits(const std::string& s);

    ClientCore& core_;
    ScenarioOptions opts_;
    std::map<std::string, std::string> vars_;
    std::vector<Step> steps_;
    std::string fail_reason_;
    int fail_line_ = 0;

    // `wait text … consume` 命中行的全文。命中行随即被消费游标丢出缓冲，
    // 紧跟其后的 `capture` 只能读这里；为空表示上一个 wait 没带 consume，
    // capture 沿用「全缓冲倒序扫」的原行为（存量剧本零改动）。
    std::string consumed_line_;

    // 计时状态（start_tp_ 在 run() 开头置位，回显前缀读它）
    std::string script_root_;
    std::chrono::steady_clock::time_point start_tp_{};
    std::string open_scenario_;
    double seg_start_sec_ = 0.0;
    std::map<std::string, double> scenario_sec_;
};

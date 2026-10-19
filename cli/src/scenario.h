#pragma once

#include <map>
#include <string>
#include <vector>

class ClientCore;

/// 无头剧本运行参数。
struct ScenarioOptions {
    std::string script_path;   // 剧本文件（必填）
    std::string report_path;   // 可选：结果报告输出路径
    std::string vars_path;     // 可选：变量文件（key=value），运行前加载、捕获后写回
    int default_timeout_ms = 15000;  // 单个 wait/assert 的默认超时
    bool echo = true;                // 是否把剧本步骤与服务器输出回显到 stdout
};

/// 运行一个无头剧本。返回 0 表示全部通过，非 0 表示失败（详见 report 输出）。
int run_scenario(ClientCore& core, const ScenarioOptions& opts);

/**
 * @file LogMonitorMessage.cpp
 * @brief 监控链路的文本产出：级别名、opencode 分析提示词与飞书正文。 */

#include "LogMonitor.h"

// 级别名（与 LogLevel 枚举同序）
static const char* kLevelLower[]
    = {"trace", "debug", "info", "warn", "error", "fatal"};
static const char* kLevelUpper[]
    = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

const char* level_lower_name(int level)
{
    return (level >= 0 && level <= LOG_LVL_FATAL) ? kLevelLower[level]
                                                  : "unknown";
}

const char* level_upper_name(int level)
{
    return (level >= 0 && level <= LOG_LVL_FATAL) ? kLevelUpper[level]
                                                  : "UNKNOWN";
}

// ---------------------------------------------------------------------
// 提示词与推送正文
// ---------------------------------------------------------------------

std::string build_analysis_prompt(const LogMonitorConfig& config)
{
    // 提示词只描述任务与产出格式：日志上下文走 --file 附件，
    // 命令行长度不随日志规模增长。
    std::string prompt
        = "你是游戏服务器「金剑江湖」（C++/Lua MUD 引擎）的值班工程师。"
          "随附文件是服务器最近的日志片段，其中出现了 ";
    prompt += level_upper_name(config.level);
    prompt += " 及以上级别的新问题。请分析日志，定位问题与根因，产出"
              "一份可执行的修复计划，用 Markdown 输出，包含：问题现象、"
              "根因分析、修复步骤（按序、可直接执行）、验证方式。"
              "只输出计划本身，不要输出无关说明。";
    return prompt;
}

// UTF-8 安全截断：从切点回退过续字节（10xxxxxx），不切碎多字节字符
static std::string truncate_utf8(const std::string& s, size_t max_bytes)
{
    if (s.size() <= max_bytes) return s;
    size_t cut = max_bytes;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
        cut--;
    }
    return s.substr(0, cut);
}

std::string build_feishu_message(const std::string& body,
                                 const std::string& body_path,
                                 const std::string& trigger_line, int level)
{
    // 飞书 text 消息约 30KB 上限：28KB 截断留余量
    const size_t kMaxBytes = 28 * 1024;

    std::string head = "【金剑江湖·日志告警】";
    head += level_upper_name(level);
    head += "\n触发日志: ";
    head += truncate_utf8(trigger_line, 500);
    head += "\n\n";

    // 直推模式没有落盘产物：省略路径尾巴，不报一个不存在的文件
    std::string tail;
    if (!body_path.empty()) tail = "\n\nplan 文件: " + body_path;
    std::string msg = head + body + tail;

    if (msg.size() > kMaxBytes) {
        // 截断会把尾部的路径一并切掉：截断注记里重新带上路径，
        // 超长消息仍可溯源到落盘文件
        std::string note = "\n\n（正文超长已截断";
        if (!body_path.empty()) note += "，完整 plan 见: " + body_path;
        note += "）";
        msg = truncate_utf8(msg, kMaxBytes - note.size()) + note;
    }
    return msg;
}

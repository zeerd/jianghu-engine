/**
 * @file LuaSmokeCheck.h
 * @brief 钩子冒烟：在校验进程里复现引擎对脚本的入口调用，把运行期错误前移。
 *
 * 静态防线只能证明"名字在某个集合里存在"；本模块把引擎本来就会做的那几次
 * 调用（item 的 create / do_use）在提交前真跑一遍。三类现场一律记为违规：
 *   - 钩子直接抛出的错误（引擎侧 lua_pcall 会吞掉，只写日志）
 *   - 被脚本自己的 pcall / xpcall 吞掉的错误
 *   - 模块体执行期错误（load_script 返回 NOREF）
 */
//---------------------------------------------------------

#pragma once

#include <string>
#include <vector>

struct lua_State;

// 一条冒烟违规：file 是被检文件原路径，hook 是出错位置
// （load = 模块体执行期，create/do_use = 钩子执行期，pcall = 被脚本吞掉）
struct SmokeViolation {
    std::string file;
    std::string hook;
    std::string message;
};

struct SmokeReport {
    int scripts          = 0;  // 参与冒烟的 item 脚本数
    int skipped          = 0;  // 跳过数（被 require 的是模块，不是数据键）
    int hooks            = 0;  // 实际驱动的钩子次数
    long long elapsed_ms = 0;  // 整轮墙钟耗时
    long long slowest_ms = 0;
    std::string slowest_script;
    std::vector<SmokeViolation> violations;
};

// 对 files 中属于 item 的脚本驱动 create / do_use。
// L 须是已 init 的引擎状态（g_LuaEngine 可用）。返回前已把替换过的
// pcall / xpcall 与脚本表字段还原，引擎状态可继续使用。
SmokeReport smoke_check_items(lua_State* L,
                              const std::vector<std::string>& files);

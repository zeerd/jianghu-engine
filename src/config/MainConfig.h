/**
 * @file MainConfig.h
 * @brief 服务器核心配置常量与编译选项。
 */

// 不同服务器的配置

#pragma once

#include <cstdint>
#include <limits>

// MAIN_VERSION 优先取构建前由 tools/gen-version.ps1 生成的 VersionAuto.h
// （格式 GS<commit日期>-<短commit>，如 GS20251028-6514d63）。
// 未运行生成步骤（如 IDE 直接编译、非 git 环境）时回退到下方固定值。
#if defined(__has_include)
#if __has_include("VersionAuto.h")
#include "VersionAuto.h"
#endif
#endif

#ifndef MAIN_VERSION
#define MAIN_VERSION "GS20251028-dev"
#endif

#define MAIN_NAME g_server_name

#pragma warning(disable : 4786)  // UNIX版本注释掉此句

// 映射槽位的整数值类型：CMapping 的数值槽、CMainObject 的 query/set 系列、
// Lua 绑定的取栈推栈，三层共用这一个别名，编译开关一档全体同宽。
// 任一层单独改宽都会在相邻层产生收窄（一边宣称 64 位、一边静默截回 32 位），
// 因此槽位类型必须跟随本别名，不得再写死 long/int32_t。
// 64 位构建槽位 8 字节，指针与句柄得以原生存放；32 位构建 4 字节，零开销。
#if defined(_WIN64) || defined(__LP64__)
using mapping_value_t = int64_t;
#else
using mapping_value_t = int32_t;
#endif

// 平台相关的零值字面量：避免在 64 位构建中用 0l（32 位 long）导致类型不匹配。
// 32 位：0l (long)；64 位：0ll (long long)。
// 使用函数式宏返回 mapping_value_t 类型，避免与 const char* 重载产生歧义。
#if defined(_WIN64) || defined(__LP64__)
#define MAPPING_VALUE_ZERO mapping_value_t(0ll)
#else
#define MAPPING_VALUE_ZERO mapping_value_t(0l)
#endif

// 槽位值域上限：防溢出守卫（余额、经验封顶）以它为界。由 mapping_value_t
// 推导、与别名同源，平台一换就一起换——守卫与槽位不会再各宽各的。
// 括号包住 max 是为了避开 Windows 头里的 max 宏。
#define MAPPING_VALUE_MAX (std::numeric_limits<mapping_value_t>::max)()

// ---------------------------------------------------------------------
// 数据域类型
//
// 数值一旦进槽就是 mapping_value_t，读出来按「数据域」承载，两族别名
// 各自登记：改某一域的宽度只动这里，不再逐点改 int/int32_t。
//
// 槽位域族（跟随 mapping_value_t）：量级无上界或已逼近 32 位上限，
// 全链路用到底，读出后不再收窄。
// 有界域族（固定 int32_t）：设计上有明确上界，或是下标/句柄/枚举，
// 收窄只发生在进入该域的那一个入口。
// ---------------------------------------------------------------------

// --- 槽位域族：随 mapping_value_t 加宽 ---
using exp_t    = mapping_value_t;  // 经验：combat_exp / sk_exp / 可用经验
using money_t  = mapping_value_t;  // 金钱：balance / 金额 / 物品价值
using hp_t     = mapping_value_t;  // 生命与法力：hp / mp / max_hp / eff_*
using damage_t = mapping_value_t;  // 伤害与治疗量
using timestamp_t
    = mapping_value_t;             // 时间戳：kill/free / begin_time / mud_age
using amount_t = mapping_value_t;  // 数量与重量：amount / weight / count
using count_t  = mapping_value_t;  // 计数：battle_count / 技能点 / 各类累计

// --- 有界域族：设计上不随槽位加宽，固定 int32_t ---
using level_t        = int32_t;  // 等级：NORMAL_MAX_LEVEL 封顶
using skill_level_t  = int32_t;  // 技能等级：level2skill 对照表域
using generation_t   = int32_t;  // 门派世代
using team_index_t   = int32_t;  // 队伍序号：FIGHT_TEAM_A/B 下标
using flag_t         = int32_t;  // 开关标记：0/1 语义
using wizard_level_t = int32_t;  // 巫师权限：grant 等级
using object_id_t    = int32_t;  // 对象 id：句柄表键，与 m_object_id 同宽
using ratio_t        = int32_t;  // 百分比比率：eff_status_msg 等

#define NORMAL_MAX_LEVEL 176

#define TOP_WEAPON_DMG 500

#define JINSHE_BU
#define CJ_104_YTJ
#define TAISHAN_JINSHE

// #define MORE_LOGS

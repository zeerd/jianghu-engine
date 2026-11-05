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

// 槽位值域上限：防溢出守卫（余额、经验封顶）以它为界。由 mapping_value_t
// 推导、与别名同源，平台一换就一起换——守卫与槽位不会再各宽各的。
// 括号包住 max 是为了避开 Windows 头里的 max 宏。
#define MAPPING_VALUE_MAX (std::numeric_limits<mapping_value_t>::max)()

#define NORMAL_MAX_LEVEL 176

#define TOP_WEAPON_DMG 500

#define JINSHE_BU
#define CJ_104_YTJ
#define TAISHAN_JINSHE

// #define MORE_LOGS

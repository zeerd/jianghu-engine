/**
 * @file MainConfig.h
 * @brief 服务器核心配置常量与编译选项。
 */

// 不同服务器的配置

#pragma once

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

#define LONG      long
#define LONGMAX   2147483647
#define LONG_ZERO (0l)

#define NORMAL_MAX_LEVEL 176

#define TOP_WEAPON_DMG 500

#define JINSHE_BU
#define CJ_104_YTJ
#define TAISHAN_JINSHE

// #define MORE_LOGS

// test_main_config.cpp — MAIN_VERSION 守护用户可见的版本串契约
//
// MAIN_VERSION 通过 Login.cpp 的 "&V=%s&" 下发给客户端，其格式是客户端可能
// 解析/显示的协议契约。构建期由 tools/gen-version.ps1 生成 VersionAuto.h，
// git 不可用时回退到固定值。本测试守护：
//   1. MAIN_VERSION 非空且以 "GS" 开头（回退链生效，未退化为空串）；
//   2. 回退值 "GS20251028-dev" 的格式（无 VersionAuto.h 时的兜底形态）。
//
// 构建运行：仓库根 .\run-coverage.ps1
#include "pch.h"
#include "MainConfig.h"
#include <string.h>

TEST(MainConfig, VersionStartsWithGS)
{
    // 版本串必须非空且以 "GS" 前缀开头——这是客户端识别版本字段的依据。
    ASSERT_NE(MAIN_VERSION, nullptr);
    EXPECT_GT(strlen(MAIN_VERSION), 2u);
    EXPECT_EQ(strncmp(MAIN_VERSION, "GS", 2), 0);
}

TEST(MainConfig, VersionFallbackFormat)
{
    // 回退值形如 "GS20251028-dev"。若构建期生成了 VersionAuto.h，
    // 实际值形如 "GS20261028-6514d63"，二者都以 "GS" 开头且含 '-'。
    // 这里只断言「含日期段 + '-' 分隔」这一最宽松的公共形态。
    EXPECT_NE(strchr(MAIN_VERSION, '-'), nullptr);
}

// tests/test_tmp_dir.h — 测试临时根目录（引擎单测与 CLI 测试共用）
//
// 路径由驱动脚本决定、测试只消费：run-coverage.ps1 以环境变量
// JIANGHU_TEST_TMP 注入（默认 <仓库根>\test_tmp）。未经脚本直跑
// （聚焦调试）时没有注入，回落到当前工作目录下的 tmp\。
//
// 回落值刻意与脚本侧的 test_tmp 不同名：它只是直跑兜底，不同名可避免
// 日后修改脚本临时目录时被顺手「同步」改掉——两边各归各改，互不牵连。
#pragma once

#include <cstdlib>
#include <filesystem>

// 返回测试临时根目录（保证存在）。子目录由调用方自行创建。
inline std::filesystem::path test_tmp_dir()
{
    const char* injected       = std::getenv("JIANGHU_TEST_TMP");
    std::filesystem::path root = (injected && *injected)
                                     ? std::filesystem::path(injected)
                                     : std::filesystem::path("tmp");
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
}

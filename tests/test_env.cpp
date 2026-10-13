// test_env.cpp — Global test environment setup
//
// Initializes g_ItemMgr with CMockItemMgr for all tests.
// Loads test configuration from test_config.ini.
// This file is compiled into tests.exe and linked with the mock layer.
//
#include "pch.h"

#include "ItemMgr.h"
#include "Logger.h"
#include "mock/mock_itemmgr.h"
#include "IniParser.h"

// Global mock item manager instance (reused across all tests)
static CMockItemMgr g_testItemMgr;

// Test environment: load config + inject mocks before all tests
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        // Load test configuration (lua paths, server settings, etc.)
        ini_load("tests/test_config.ini");
        logger_set_level(
            logger_level_from_str(ini_get("log", "level", "trace")));
        logger_set_path(ini_get("log", "path", "log"));

        // 每次测试运行写入独立日志文件：M-D-<runid>.txt
        const std::string runId = logger_new_run_id();
        logger_set_run_id(runId.c_str());

        g_item_mgr = &g_testItemMgr;
    }
    void TearDown() override
    {
        g_item_mgr = nullptr;
        logger_set_run_id("");
        // g_ini_data 为全局值，程序退出自动析构，无需手动释放。
    }
};

// Register the environment (must be before main)
::testing::Environment* const g_testEnv
    = ::testing::AddGlobalTestEnvironment(new TestEnvironment);

// ----------------------------------------------------------------
// 验收门禁：Debug 构建的测试程序本身非法，直接按宏定义报错
//
// 生产服务器以 Release|Win32（/MT, NDEBUG, /O2）构建；Debug 的栈填充区/
// 调试堆会暂时掩盖越界写与未初始化内存，优化开关又改变 UB 的表现——
// Debug 全绿不代表生产可用（实例：dump_caller_info 的 SYMBOL_INFO 柔性
// 数组栈溢出，Debug 1216 全绿，Release 下 98 个用例 SEH 0xc0000005）。
//
// 因此本门禁不设任何运行时豁免（不看环境变量/命令行参数——否则任何
// 一次设置就能绕过）：Debug 构建下本用例无条件失败，无论以何种方式运行
// 都会在测试结果中报错。唯一认可的运行方式是 .\\run-coverage.ps1，
// 它以 Release 配置编译并采集覆盖率。
#ifdef _DEBUG
TEST(TestConfigGate, DebugBuildIsNotAcceptance)
{
    FAIL() << "This is a Debug-built tests.exe: no result from it counts "
              "as acceptance. Production is built Release|Win32 (/MT, NDEBUG, "
              "/O2) where stack padding / debug heap hide out-of-bounds writes "
              "and UB. Use .\\run-coverage.ps1 (builds Release and collects "
              "coverage).";
}
#endif  // _DEBUG

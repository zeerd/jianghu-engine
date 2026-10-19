// test_lua_backtrace.cpp — load_script 文件缺失失败路径的回溯契约
//
// 契约：脚本文件不存在时 load_script 返回 LUA_NOREF，并经 dump_caller_info
// 在日志写出该脚本的 BACKTRACE 回溯块；回溯内 SymFromAddr 解析出的符号名
// 必须写进紧随 SYMBOL_INFO 的名字缓冲（裸 SYMBOL_INFO 结构的 MaxNameLen
// 越界写曾把 Release 下 tests 自身的栈冲毁：98 用例 0xc0000005）。
//
// 失败方式即崩溃：栈被冲毁时本用例在 dump_caller_info 返回途中即 SEH，
// 测试进程以失败退出；回溯块被摘除则命中计数断言失败。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>

#include "LuaEngine.h"
#include "test_log_hits.h"
#include "test_lua_env.h"

namespace {

// 一次性 Lua 触发器：由 chunk 第 3 行调用，进入缺失脚本加载的回溯路径
int TriggerMissingScriptLoad(lua_State* L)
{
    CLuaEngine::instance()->load_script("backtrace_luafail_zzz",
                                        LUA_SCRIPT_ITEM);
    return 0;
}

}  // namespace

class LuaBacktraceTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

TEST_F(LuaBacktraceTest, MissingScriptReportsBacktraceAndKeepsEngineUsable)
{
    const char* needle = "BACKTRACE  context: [backtrace_missing_zzz]";
    const int before   = count_log_hits(needle);

    CLuaEngine* engine = CLuaEngine::instance();
    EXPECT_EQ(engine->load_script("backtrace_missing_zzz", LUA_SCRIPT_ITEM),
              LUA_NOREF);
    EXPECT_EQ(count_log_hits(needle), before + 1)
        << "缺失脚本失败路径没有写出 BACKTRACE 回溯";

    // 回溯打印后引擎必须完好：真实 item 脚本仍可加载。
    EXPECT_NE(engine->load_script("plain_test", LUA_SCRIPT_ITEM), LUA_NOREF);
}

// 回溯的 Lua 帧行号必须是真实触发行（chunk 第 3 行）。lua_getinfo 只有
// 带选项 'l' 才会写 currentline；缺了它，%d 打的是 lua_Debug 未初始化
// 字段——历史日志 :75360452 实为线程栈地址 0x047DE8C4（跨轮变化、帧间
// 相同），行号栏毫无意义。
TEST_F(LuaBacktraceTest, BacktraceReportsRealLuaLine)
{
    const char* miss_needle = "[Lua] backtrace_luafail_zzz: cannot read file";
    const int miss_before   = count_log_hits(miss_needle);

    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_NE(L, nullptr);

    lua_pushcfunction(L, TriggerMissingScriptLoad);
    lua_setglobal(L, "trigger");

    const char* chunk = "\n\ntrigger()\n";  // 触发行 = 第 3 行
    ASSERT_EQ(
        luaL_loadbuffer(L, chunk, std::strlen(chunk), "backtrace_luafail"),
        LUA_OK)
        << lua_tostring(L, -1);
    ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_OK) << lua_tostring(L, -1);

    lua_pushnil(L);
    lua_setglobal(L, "trigger");

    EXPECT_EQ(count_log_hits(miss_needle), miss_before + 1)
        << "触发器没有进入缺失脚本加载路径";

    const std::string log = read_log_content();
    EXPECT_NE(log.find("[string \"backtrace_luafail\"]:3 in"),
              std::string::npos)
        << "回溯没打出真实 Lua 行号（currentline 未被 lua_getinfo 填充）";
}

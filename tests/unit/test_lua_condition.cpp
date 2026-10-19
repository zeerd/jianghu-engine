// test_lua_condition.cpp — CLuaCondition 契约测试
//
// condition 已全部 Lua 化，C++ 侧不再提供任何回退实现：脚本必须返回 table，
// 且必须提供 update_condition 字段。核心契约分两层：
// 1. 施加层——脚本加载失败（文件缺失/语法错/chunk 错/非表返回）或 init
//    抛错时，apply_condition 拒绝写入条件项（返回 false），不产生一个心跳
//    的假状态；load_script 对同一失败记负缓存，重复施加只回一行短错误。
// 2. 心跳层——脚本已加载但契约失败（缺 update_condition 字段/update 抛错）
//    时，update_condition 返回 0 清除条件项；另覆盖正常自减的成功路径。
//
// 末尾一节锁定数据集（lua-sample）的条件脚本契约：样本脚本才是玩家真正
// 挂上的那一份，引擎契约绿了但样本写错（比如不递减 duration、归零不返回 0）
// 时，仍会表现为「毒永远不解」。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "Char.h"
#include "Condition.h"
#include "Global.h"
#include "IniParser.h"
#include "SkillMgr.h"
#include "test_log_hits.h"
#include "test_lua_env.h"
#include "test_sample_script.h"

namespace {

// 用例退出时把条件对象摘除并销毁，避免污染 g_condition_list 后续用例。
class ScopedCondition {
public:
    explicit ScopedCondition(const char* name) : m_name(name) {}
    ~ScopedCondition()
    {
        auto it = g_condition_list.find(m_name);
        if (it == g_condition_list.end()) return;
        delete it->second;
        g_condition_list.erase(it);
    }
    ScopedCondition(const ScopedCondition&)            = delete;
    ScopedCondition& operator=(const ScopedCondition&) = delete;

private:
    std::string m_name;
};

class LuaConditionTest : public ::testing::Test {
protected:
    CChar* ch = nullptr;
    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        ch = new CChar();
        ch->set("name", "cnd");
        ch->set("id", "cnd1");
    }
    void TearDown() override
    {
        // 条件脚本的持续伤害走 receive_damage，会把角色挂进全局心跳表；
        // 先摘心跳再销毁，免得心跳链表里留下悬垂指针。
        ch->set_heart_beat(0);
        delete ch;
    }

    // 心跳若干次，返回条件是否仍存活（true = 一直未被清除）
    bool tick_until_gone(const char* name, int max_ticks)
    {
        for (int i = 0; i < max_ticks; i++) {
            ch->update_condition();
            if (ch->query_condition(name) == 0) return false;
        }
        return true;
    }
};

// ---------------------------------------------------------------------
// 成功路径：脚本返回表且含 update_condition，duration 每跳自减并写回
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, NormalScript_ContinuesAndDecrements)
{
    ScopedCondition guard("poison");

    ch->apply_condition("poison", 3, 0);
    EXPECT_EQ(ch->query_condition("poison"), 3);

    ch->update_condition();
    EXPECT_EQ(ch->query_condition("poison"), 2);

    // 继续心跳直到条件被脚本返回 0 清除
    EXPECT_FALSE(tick_until_gone("poison", 10));
    EXPECT_EQ(ch->query_condition("poison"), 0);
}

// ---------------------------------------------------------------------
// 施加层：load_script 四个失败出口 + init 抛错，一律拒绝写入
// （表驱动单一 owner：同一契约不逐夹具重复写用例）
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, ScriptLoadFailure_ApplyRejected)
{
    const char* names[] = {
        "tc_missing_zzz",    // 无法读文件（文件不存在）
        "tc_no_table",       // 非表返回
        "tc_runtime",        // chunk 执行期 error
        "tc_init_error",     // 脚本表 init 抛错
    };
    for (const char* name : names) {
        ScopedCondition guard(name);
        EXPECT_FALSE(ch->apply_condition(name, 5, 0)) << name;
        EXPECT_EQ(ch->query_condition(name), 0)
            << "拒绝写入后条件项不应存在: " << name;
        // 心跳也不产生任何残留
        ch->update_condition();
        EXPECT_EQ(ch->query_condition(name), 0) << name;
    }
}

// ---------------------------------------------------------------------
// 失败路径 2：返回表但缺 update_condition 字段 → 条件被清除
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, MissingUpdateFunc_ConditionCleared)
{
    ScopedCondition guard("tc_nofunc");

    ch->apply_condition("tc_nofunc", 5, 0);
    ASSERT_GT(ch->query_condition("tc_nofunc"), 0);

    ch->update_condition();
    EXPECT_EQ(ch->query_condition("tc_nofunc"), 0);
}

// ---------------------------------------------------------------------
// 失败路径 3：update_condition 运行期 error → 条件被清除
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, UpdateFuncRuntimeError_ConditionCleared)
{
    ScopedCondition guard("tc_error");

    ch->apply_condition("tc_error", 5, 0);
    ASSERT_GT(ch->query_condition("tc_error"), 0);

    ch->update_condition();
    EXPECT_EQ(ch->query_condition("tc_error"), 0);
}

// ---------------------------------------------------------------------
// 心跳层：孤儿键——条件项存在但脚本缺失（如旧存档恢复的残留键），
// 无有效脚本引用的条件对象经基类早退返回 0，一拍安全清除
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, OrphanKeyWithoutScript_SafelyClearedByHeartbeat)
{
    ScopedCondition guard("tc_orphan_zzz");

    // 施加失败（脚本缺失），但条件对象已入缓存且 m_script_ref 无效
    ASSERT_FALSE(ch->apply_condition("tc_orphan_zzz", 5, 0));
    // 模拟存档残留：直接写入条件键
    ch->set("condition/tc_orphan_zzz", (long)5);
    ASSERT_EQ(ch->query_condition("tc_orphan_zzz"), 5);

    ch->update_condition();
    EXPECT_EQ(ch->query_condition("tc_orphan_zzz"), 0);
}

// ---------------------------------------------------------------------
// 施加层：负缓存——首次失败打完整 BACKTRACE，重复施加只回一行短错误
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, LoadFailure_NegativeCacheSuppressesBacktrace)
{
    ScopedCondition guard("tc_cache_zzz");
    const char* backtrace = "BACKTRACE  context: [tc_cache_zzz]";
    const char* short_err = "[Lua] tc_cache_zzz: cannot read file";

    const int bt0  = count_log_hits(backtrace);
    const int err0 = count_log_hits(short_err);

    // 首次施加：完整回溯 + 1
    EXPECT_FALSE(ch->apply_condition("tc_cache_zzz", 5, 0));
    EXPECT_EQ(count_log_hits(backtrace), bt0 + 1);
    EXPECT_EQ(count_log_hits(short_err), err0 + 1);

    // 重复施加：命中负缓存，回溯不再增长，只多一行短错误
    EXPECT_FALSE(ch->apply_condition("tc_cache_zzz", 5, 0));
    EXPECT_EQ(count_log_hits(backtrace), bt0 + 1)
        << "负缓存未生效：重复施加又打了 BACKTRACE";
    EXPECT_EQ(count_log_hits(short_err), err0 + 2);
}

// ---------------------------------------------------------------------
// 施加层：补上脚本文件后无需重启即恢复（负缓存按内容 hash 自动重试）
// ---------------------------------------------------------------------
TEST_F(LuaConditionTest, LoadFailure_RecoversWhenScriptFileAppears)
{
    namespace fs           = std::filesystem;
    const std::string root = ini_get("lua", "script_root", "");
    ASSERT_FALSE(root.empty());
    const fs::path file = fs::path(root) / "condition" / "tc_late_zzz.lua";
    fs::remove(file);  // 清理上次残留

    ScopedCondition guard("tc_late_zzz");

    // 文件缺失：拒绝写入（同时在引擎记下负缓存）
    ASSERT_FALSE(ch->apply_condition("tc_late_zzz", 5, 0));
    EXPECT_EQ(ch->query_condition("tc_late_zzz"), 0);

    // 文件补上：下一次施加即成功，无需重启/无需 reload 命令
    {
        std::ofstream out(file);
        out << "return {\n"
               "    update_condition = function(self, me, duration)\n"
               "        if duration < 1 then return 0 end\n"
               "        return CND_CONTINUE, duration - 1\n"
               "    end,\n"
               "}\n";
    }
    EXPECT_TRUE(ch->apply_condition("tc_late_zzz", 5, 0));
    EXPECT_EQ(ch->query_condition("tc_late_zzz"), 5);

    ch->update_condition();
    EXPECT_EQ(ch->query_condition("tc_late_zzz"), 4);

    ch->apply_condition("tc_late_zzz", 0, 0);  // 先清条件再删文件
    fs::remove(file);
}

// ---------------------------------------------------------------------
// 样本条件脚本契约：lua-sample/condition/sample_poison.lua
//
// 引擎只认自己的脚本根（test_config.ini 的 script_root），而条件名必须与
// 条件脚本同名，故用例把样本脚本原文拷进单测脚本根再交给引擎加载——
// 拷原文用共享夹具 ScopedSampleScript（test_sample_script.h）。
// ---------------------------------------------------------------------

// 样本条件脚本：挂载即写入条件项，逐拍扣血并递减 duration，归零后引擎清除条件项
TEST_F(LuaConditionTest, SampleScript_MountsTicksAndExpires)
{
    ScopedSampleScript script("condition/sample_poison.lua",
                              "lua-sample/condition/sample_poison.lua");
    ASSERT_TRUE(script.ok()) << "样本条件脚本未就位: " << script.file();
    ScopedCondition guard("sample_poison");

    ch->set("max_hp", (long)100);
    ch->set("hp", (long)100);
    ch->set("eff_hp", (long)100);

    // 施加层：脚本可用 → init 被调用 → 条件项写入（2 拍）
    ASSERT_TRUE(ch->apply_condition("sample_poison", 2, 0));
    EXPECT_EQ(ch->query_condition("sample_poison"), 2);
    EXPECT_EQ(ch->query("hp"), 100) << "挂载本身不扣血";

    // 心跳层第 1 拍：脚本扣掉 5% 气血上限的血量，duration 自减
    ch->update_condition();
    EXPECT_EQ(ch->query_condition("sample_poison"), 1);
    EXPECT_EQ(ch->query("hp"), 95) << "毒性每拍应扣 5% 气血上限";

    // 心跳层第 2 拍：duration 归零 → 脚本返回 0 → 引擎清除条件项
    ch->update_condition();
    EXPECT_EQ(ch->query_condition("sample_poison"), 0);
    EXPECT_EQ(ch->query("hp"), 95) << "毒性散尽那拍不再扣血";
}

}  // namespace

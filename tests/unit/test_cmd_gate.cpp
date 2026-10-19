// test_cmd_gate.cpp — 命令准询问点（command_gate）测试
//
// 契约覆盖两层：
//   1. 规则层（hooks.combat_rule, phase = "command_gate"）：月屏魔咒免战保护
//      —— target 带 ypmz_protected temp 且命令属于攻击性命令（kill/fight/cast）
//      → {reason=文案}；否则 nil。数据键 ypmz_protected 由 Lua 物品脚本
//      （item/goods/yueping.lua）写入，C++ 不再感知该键。
//   2. 站点层（CCmdHandle::command 角色目标拦截之前）：{reason} 非空即吞掉
//      本次命令（返回 1、命令未被执行），空则照常落到 handle_action 分发链。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "CombatRules.h"
#include "Global.h"
#include "Npc.h"
#include "Room.h"
#include "ServerConfig.h"
#include "User.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];
extern int g_current_time;

namespace {

class CommandGateTest : public ::testing::Test {
protected:
    void SetUp() override { ensure_lua_env(); }

    static CUser* MakeUser(const char* name, const char* id)
    {
        CUser* u = new CUser();
        u->set_name(name, id);
        u->set("id", id);
        return u;
    }
};

// ---------------------------------------------------------------------------
// 规则层：dataset 的 command_gate 分支
// ---------------------------------------------------------------------------
TEST_F(CommandGateTest, CommandGate_UnprotectedTarget_AllowsAttackCommands)
{
    CUser* me     = MakeUser("常人", "cg_atk2");
    CUser* target = MakeUser("普通玩家", "cg_vic2");

    EXPECT_TRUE(combat_rule_reason("command_gate", me, target, "kill").empty());

    delete me;
    delete target;
}

TEST_F(CommandGateTest, CommandGate_NonAttackCommand_Allowed)
{
    CUser* me     = MakeUser("施咒者", "cg_atk3");
    CUser* target = MakeUser("免战者", "cg_vic3");
    target->set_temp("ypmz_protected", 1);

    EXPECT_TRUE(combat_rule_reason("command_gate", me, target, "look").empty());

    delete me;
    delete target;
}

TEST_F(CommandGateTest, CommandGate_DetailNil_Allowed)
{
    CUser* me     = MakeUser("施咒者", "cg_atk4");
    CUser* target = MakeUser("免战者", "cg_vic4");
    target->set_temp("ypmz_protected", 1);

    // 无上下文的询问点（detail = nil）不属于攻击命令
    EXPECT_TRUE(combat_rule_reason("command_gate", me, target).empty());

    delete me;
    delete target;
}

// 钩子缺席回退由 CLuaHookCall
// 统一保证（owner：test_lua_hook.cpp），不在此重放。

// ---------------------------------------------------------------------------
// 站点层：CCmdHandle::command 消费 {reason}
// ---------------------------------------------------------------------------
class CommandGateDispatchTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        g_notify_fail_msg[0] = '\0';

        room = new CRoom();
        room->set_name("测试场地", "cg_room");
        // 禁战：未被免战规则吞掉的攻击命令在此早退，不产生战斗副作用
        room->set("no_fight", 1);

        me = new CChar();
        me->set("name", "动手的人");
        me->set("id", "cg_me");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("eff_hp", (long)100);
        me->set("max_hp", (long)100);
        me->set("combat_exp", (long)100000);
        me->move(room);

        npc = new CNpc();
        npc->set("name", "免战商人");
        npc->set("id", "cg_npc");
        npc->set("hp", (long)100);
        npc->set("eff_hp", (long)100);
        npc->set("max_hp", (long)100);
        npc->move(room);
    }

    void TearDown() override
    {
        npc->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete npc;
        delete me;
        delete room;
    }

    // 完整命令分发链（角色目标拦截 + command_gate 询问）
    int Send(const char* cmd)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s %ld", cmd, npc->object_id());
        return me->command(buf);
    }

    CRoom* room = nullptr;
    CChar* me   = nullptr;
    CNpc* npc   = nullptr;
};

// 对照组：无免战保护时规则放行，kill 照常进入后续链（由禁战房拒绝）
TEST_F(CommandGateDispatchTest, Kill_UnprotectedNpc_PassesGate)
{
    EXPECT_EQ(Send("kill"), 0);
    EXPECT_STREQ(g_notify_fail_msg, "此地禁止战斗!")
        << "失败来自禁战房，说明 command_gate 未拦截";
}

}  // namespace

// ---------------------------------------------------------------------------
// command() 限流：5秒内第16条 → 警告；旧窗口 → 刷新时刻放行
// ---------------------------------------------------------------------------
TEST_F(CommandGateDispatchTest, Command_Flood_WarnsWithinWindow)
{
    auto* u = new CUser();
    u->set_name("刷屏者", "cg_flood1");
    u->set("id", "cg_flood1");
    u->set("hp", (long)100);
    u->move(room);
    u->delete_temp("client/loading");
    u->set_temp("cmd/cnt", 15);
    u->set_temp("cmd/warning", 0);
    u->set_temp("cmd/time", (long)g_current_time);  // 5 秒窗口内

    EXPECT_EQ(0, u->command((char*)"look")) << "第 16 条被吞(208-209)";
    EXPECT_EQ(1, u->query_temp("cmd/warning")) << "首违升级为警告(190-197)";
    EXPECT_EQ(g_current_time, u->query_temp("cmd/time")) << "窗口时刻刷新(208)";
    EXPECT_EQ(0, u->query_temp("cmd/cnt")) << "计数清零再计(185-186)";

    u->move(nullptr);
    delete u;
}

TEST_F(CommandGateDispatchTest, Command_FloodOldWindow_FallsThrough)
{
    auto* u = new CUser();
    u->set_name("间歇刷屏", "cg_flood2");
    u->set("id", "cg_flood2");
    u->set("hp", (long)100);
    u->move(room);
    u->delete_temp("client/loading");
    u->set_temp("cmd/cnt", 15);
    u->set_temp("cmd/time", (long)(g_current_time - 10));  // 5 秒窗外

    u->command((char*)"look");
    EXPECT_EQ(g_current_time, u->query_temp("cmd/time"))
        << "旧窗口只刷新时刻不警告(212)";
    EXPECT_EQ(0, u->query_temp("cmd/warning")) << "窗外不升级警告";

    u->move(nullptr);
    delete u;
}

// 关闭限流（config.ini [server] rate_limit=0，E2E 集成测试配置）：
// 窗口内第 16 条命令照常分发，计数器与警告均不触发。
TEST_F(CommandGateDispatchTest, Command_RateLimitOff_BoundaryPasses)
{
    g_ini_data["server"]["rate_limit"] = "0";
    CServerConfig::init();

    auto* u = new CUser();
    u->set_name("停用限流", "cg_nolimit");
    u->set("id", "cg_nolimit");
    u->set("hp", (long)100);
    u->move(room);
    u->delete_temp("client/loading");
    u->set_temp("cmd/cnt", 15);
    u->set_temp("cmd/warning", 0);
    u->set_temp("cmd/time", (long)g_current_time);  // 5 秒窗口内

    char buf[64];
    snprintf(buf, sizeof(buf), "kill %ld", npc->object_id());
    EXPECT_EQ(0, u->command(buf));
    EXPECT_STREQ(g_notify_fail_msg, "此地禁止战斗!")
        << "分发走到禁战房，说明未被限流丢弃";
    EXPECT_EQ(0, u->query_temp("cmd/warning")) << "关闭时不再发警告";
    EXPECT_EQ(15, u->query_temp("cmd/cnt")) << "关闭时计数保持不动";

    g_ini_data["server"]["rate_limit"] = "1";
    CServerConfig::init();
    u->move(nullptr);
    delete u;
}

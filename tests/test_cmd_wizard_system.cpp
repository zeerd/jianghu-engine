// test_cmd_wizard_system.cpp - Unit tests for CmdWizardSystem.cpp
//
// Tests game state underlying the wizard command handlers.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "Skill.h"
#include "Channel.h"
#include "CmdWizard.h"
#include "UserMgr.h"  // add_user/remove_user（call delete family 目标）
#include "CombatConfig.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"
#include "test_char_common.h"
#include "mock/mock_itemmgr.h"
#include "Logger.h"

extern char g_notify_fail_msg[512];

class CmdWizardSystemTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallFull(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"full", me, target, arg);
    }
    static int CallSummon(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"summon", me, target, arg);
    }
    static int CallGoto(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"goto", me, target, arg);
    }
    static int CallStopBattle(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"stop", me, target, arg);
    }
    static int CallSysParam(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"systemparam", me, target, arg);
    }
    static int CallSave(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"force_save", me, target, arg);
    }
    static int CallSetLog(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"set_log", me, target, arg);
    }

    static int call_wiz(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"call", me, target, arg);
    }

    static int call_skill_param(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"skillparam", me, target, arg);
    }

    static int call_set_skill(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"set_skill", me, target, arg);
    }

    static int call_restore_exp(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"restore_exp", me, target, arg);
    }

    static int call_clear_skill(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"clear_skill", me, target, arg);
    }

    static int call_recruit_exp(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"recruit_exp", me, target, arg);
    }

    static int call_reward_exp(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"reward_exp", me, target, arg);
    }

    static int call_sub_exp(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"sub_exp", me, target, arg);
    }

    static int call_reload_lua(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"reload", me, target, arg);
    }

    static int call_print_mapping(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"print_mapping", me, target, arg);
    }

    static int call_update(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"update", me, target, arg);
    }

    CChar* me;
    CChar* other;
    CContainer* room;
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CharMockDb* db      = nullptr;
    LogLevel saved_level;

    void SetUp() override
    {
        ensure_lua_env();
        // 生产由 init_game 建频道；巫师命令会直接播报 sys 频道
        g_channel.create();
        saved_mgr            = g_item_mgr;
        g_item_mgr           = &item_mgr;
        db                   = new CharMockDb();
        g_db                 = db;
        saved_level          = logger_get_level();
        g_notify_fail_msg[0] = '\0';
        room                 = new CContainer();
        room->set("name", "test_room");
        room->set("wiz_room", (long)1);

        me = new CChar();
        me->set("name", "测试巫师");
        me->set("id", "wiz1");
        me->set("level", (long)100);
        me->set("grant", (long)200);
        me->set("hp", (long)1000);
        me->set("max_hp", (long)1000);
        me->set("mp", (long)500);
        me->set("max_mp", (long)500);
        me->set("combat_exp", (long)10000000);
        me->move(room);

        other = new CChar();
        other->set("name", "测试玩家");
        other->set("id", "player1");
        other->set("level", (long)50);
        other->set("hp", (long)50);
        other->set("max_hp", (long)100);
        other->set("mp", (long)25);
        other->set("max_mp", (long)50);
        other->set("eff_hp", (long)50);
        other->set("eff_mp", (long)25);
        other->set("combat_exp", (long)100000);
        other->move(room);
    }

    void TearDown() override
    {
        logger_set_level(saved_level);
        other->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete other;
        delete me;
        delete room;
        g_db = nullptr;
        delete db;
        g_item_mgr = saved_mgr;
    }
};

TEST_F(CmdWizardSystemTest, Full_NoTarget)
{
    int result = CallFull(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardSystemTest, Full_ValidTarget)
{
    int result = CallFull(me, other->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_EQ(other->query("hp"), other->query("max_hp"));
    EXPECT_EQ(other->query("mp"), other->query("max_mp"));
}

TEST_F(CmdWizardSystemTest, Summon_NoTarget)
{
    int result = CallSummon(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardSystemTest, Goto_NoArgs)
{
    int result = CallGoto(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardSystemTest, StopBattle_NotFightRoom)
{
    int result = CallStopBattle(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardSystemTest, SysParam_SetAndQuery)
{
    // systemparam set/query 完整链路：写入配置并可读回（返回 1）
    char setArg[512] = "set test_cfg_key 42";
    EXPECT_EQ(CallSysParam(me, 0, setArg), 1);
    EXPECT_EQ(g_combat_config.query("test_cfg_key"), 42);

    char queryArg[512] = "query test_cfg_key";
    EXPECT_EQ(CallSysParam(me, 0, queryArg), 1);
}

TEST_F(CmdWizardSystemTest, RewardExp_NoTarget)
{
    int result = CallFull(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdWizardSystemTest, Save_NoTarget)
{
    int result = CallSave(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
}

// ====================================================================
// systemparam 保留键 log_level：读写运行时日志级别
// ====================================================================

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_SetNumeric)
{
    char arg[512] = "set log_level 3";
    EXPECT_EQ(CallSysParam(me, 0, arg), 1);
    EXPECT_EQ(logger_get_level(), LOG_LVL_WARN);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_SetstrName)
{
    char arg[512] = "setstr log_level info";
    EXPECT_EQ(CallSysParam(me, 0, arg), 1);
    EXPECT_EQ(logger_get_level(), LOG_LVL_INFO);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_SetNameViaSet)
{
    // set 动作传非数字值时按级别名解析
    char arg[512] = "set log_level warn";
    EXPECT_EQ(CallSysParam(me, 0, arg), 1);
    EXPECT_EQ(logger_get_level(), LOG_LVL_WARN);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_Query)
{
    logger_set_level(LOG_LVL_ERROR);
    char arg[512] = "query log_level";
    EXPECT_EQ(CallSysParam(me, 0, arg), 1);
    // 查询不得修改级别
    EXPECT_EQ(logger_get_level(), LOG_LVL_ERROR);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_Querystr)
{
    logger_set_level(LOG_LVL_FATAL);
    char arg[512] = "querystr log_level";
    EXPECT_EQ(CallSysParam(me, 0, arg), 1);
    EXPECT_EQ(logger_get_level(), LOG_LVL_FATAL);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_InvalidName)
{
    logger_set_level(LOG_LVL_INFO);
    char arg[512] = "setstr log_level xyz";
    EXPECT_EQ(CallSysParam(me, 0, arg), 0);
    // 非法级别名必须被拒绝且级别保持不变（不得静默回退 TRACE）
    EXPECT_EQ(logger_get_level(), LOG_LVL_INFO);
    EXPECT_STREQ(g_notify_fail_msg, "无效日志级别：xyz。");
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_OutOfRange)
{
    logger_set_level(LOG_LVL_INFO);
    char arg[512] = "set log_level 9";
    EXPECT_EQ(CallSysParam(me, 0, arg), 0);
    EXPECT_EQ(logger_get_level(), LOG_LVL_INFO);

    char negArg[512] = "set log_level -1";
    EXPECT_EQ(CallSysParam(me, 0, negArg), 0);
    EXPECT_EQ(logger_get_level(), LOG_LVL_INFO);
}

TEST_F(CmdWizardSystemTest, SysParam_LogLevel_DeleteRejected)
{
    char arg[512] = "delete log_level";
    EXPECT_EQ(CallSysParam(me, 0, arg), 0);
}

TEST_F(CmdWizardSystemTest, SetLog_CommandRemoved)
{
    // set_log 不是已注册命令：分发器找不到，返回 -1
    EXPECT_EQ(CallSetLog(me, 0, (char*)"info"), -1);
}

extern MAP2USER g_user_list;

// ---------------- call：巫师改查任意对象 ----------------
TEST_F(CmdWizardSystemTest, Call_OnEnvironment_SetsNumberField)
{
    char arg[256] = "set level 77";
    EXPECT_EQ(call_wiz(me, 0, arg), 1);
    EXPECT_EQ(room->query("level"), 77) << "无目标即落在所处环境上";
}

TEST_F(CmdWizardSystemTest, Call_Set_NameAndId)
{
    char rename[256] = "set name 新名字";
    EXPECT_EQ(call_wiz(me, 0, rename), 1);
    EXPECT_STREQ(room->name(), "新名字");

    char reid[256] = "set id new_room_id";
    EXPECT_EQ(call_wiz(me, 0, reid), 1);
    EXPECT_STREQ(room->querystr("id"), "new_room_id");
}

TEST_F(CmdWizardSystemTest, Call_Set_GuardsPrivilegedFields)
{
    char grant[256]      = "set grant 5";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_wiz(me, 0, grant), 0);
    EXPECT_STREQ(g_notify_fail_msg, "请使用promote。");

    char points[256]     = "set 点数 5";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_wiz(me, 0, points), 0);
    EXPECT_STREQ(g_notify_fail_msg, "请使用增值服务支票。");
}

TEST_F(CmdWizardSystemTest, Call_Delete_RemovesField)
{
    other->set("wiz_probe", (long)9);
    char arg[256] = "";
    snprintf(arg, 256, "delete wiz_probe");
    EXPECT_EQ(call_wiz(me, other->object_id(), arg), 1);
    EXPECT_EQ(other->query("wiz_probe"), 0);
}

TEST_F(CmdWizardSystemTest, Call_QueryReportsThroughUserClient)
{
    StateRecordingUser wiz;
    wiz.set_name("巫师", "wiz_capture");
    wiz.set("grant", (long)200);
    wiz.move(room);
    wiz.delete_temp("client/loading");
    wiz.clear();

    char num[256] = "query level";
    EXPECT_EQ(call_wiz(&wiz, other->object_id(), num), 1);
    EXPECT_TRUE(wiz.has("level=50")) << wiz.msgs.size();

    char str[256] = "querystr name";
    EXPECT_EQ(call_wiz(&wiz, other->object_id(), str), 1);
    EXPECT_TRUE(wiz.has("name=测试玩家")) << wiz.msgs.size();

    wiz.move(nullptr);
}

TEST_F(CmdWizardSystemTest, Call_TempDb_RoundTrip)
{
    char set[256] = "set_temp wiz_tmp 5";
    EXPECT_EQ(call_wiz(me, other->object_id(), set), 1);
    EXPECT_EQ(other->query_temp("wiz_tmp"), 5);

    char setstr[256] = "set_temp wiz_tmp_s 临时";
    EXPECT_EQ(call_wiz(me, other->object_id(), setstr), 1);
    EXPECT_STREQ(other->querystr_temp("wiz_tmp_s"), "临时");

    char del[256] = "delete_temp wiz_tmp";
    EXPECT_EQ(call_wiz(me, other->object_id(), del), 1);
    EXPECT_EQ(other->query_temp("wiz_tmp"), 0);
}

TEST_F(CmdWizardSystemTest, Call_UnknownTarget_Rejected)
{
    char arg[256]        = "query level";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_wiz(me, 123456, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "请选中目标。");
}

// ---------------- skillparam：改查技能参数 ----------------
TEST_F(CmdWizardSystemTest, SkillParam_SetAndQuery)
{
    CSkill* sk = load_skill("unarmed");

    char set_num[256] = "unarmed set wiz_probe 5";
    EXPECT_EQ(call_skill_param(me, 0, set_num), 1);
    EXPECT_EQ(sk->query("wiz_probe"), 5);

    char set_str[256] = "unarmed setstr wiz_probe 好手感";
    EXPECT_EQ(call_skill_param(me, 0, set_str), 1);
    EXPECT_STREQ(sk->querystr("wiz_probe"), "好手感");

    sk->del("wiz_probe");
}

TEST_F(CmdWizardSystemTest, SkillParam_QueryReportsThroughUserClient)
{
    CSkill* sk = load_skill("unarmed");
    sk->set("wiz_probe", (long)7);

    StateRecordingUser wiz;
    wiz.set_name("巫师", "wiz_skill_capture");
    wiz.set("grant", (long)200);
    wiz.move(room);
    wiz.delete_temp("client/loading");
    wiz.clear();

    char query[256] = "unarmed query wiz_probe";
    EXPECT_EQ(call_skill_param(&wiz, 0, query), 1);
    EXPECT_TRUE(wiz.has("wiz_probe=7")) << wiz.msgs.size();

    wiz.move(nullptr);
    sk->del("wiz_probe");
}

TEST_F(CmdWizardSystemTest, SkillParam_Delete)
{
    CSkill* sk = load_skill("unarmed");
    sk->set("wiz_probe", (long)3);

    char del[256] = "unarmed delete wiz_probe";
    EXPECT_EQ(call_skill_param(me, 0, del), 1);
    EXPECT_EQ(sk->query("wiz_probe"), 0);
}

// ---------------- 技能与经验类 ----------------
TEST_F(CmdWizardSystemTest, SetSkill_WritesTargetSkillLevel)
{
    char arg[256] = "unarmed 50";
    EXPECT_EQ(call_set_skill(me, other->object_id(), arg), 1);
    EXPECT_EQ(other->query_skill("unarmed", 1), 50);
}

TEST_F(CmdWizardSystemTest, RestoreExp_TriggersOnHeavySkillExpRatio)
{
    StateRecordingUser target;
    target.set_name("徒弟", "wiz_restore");
    target.set("level", (long)50);
    target.set("combat_exp", (long)100000);
    target.set("sk_exp", (long)60000);
    target.move(room);
    target.delete_temp("client/loading");
    g_user_list[target.object_id()] = &target;

    char arg[8] = "";
    EXPECT_EQ(call_restore_exp(me, target.object_id(), arg), 1);
    EXPECT_EQ(target.query("sk_exp"), 40000) << "技能经验按 2/3 收缩";

    g_user_list.erase(target.object_id());
    target.move(nullptr);
}

TEST_F(CmdWizardSystemTest, ClearSkill_WipesWholeSkillSurface)
{
    StateRecordingUser target;
    target.set_name("徒弟", "wiz_clear");
    target.set_skill("unarmed", 50);
    target.set_skill("strike", 30);
    target.query_skill_map()->set("unarmed", "strike");
    target.set("sk_exp", (long)99);
    target.move(room);
    target.delete_temp("client/loading");
    g_user_list[target.object_id()] = &target;

    char arg[8] = "";
    EXPECT_EQ(call_clear_skill(me, target.object_id(), arg), 1);
    EXPECT_EQ(target.query_skill("unarmed", 1), 0);
    EXPECT_EQ(target.query_skill("strike", 1), 0);
    EXPECT_EQ(target.query_skill_map()->get_count(), 0);
    EXPECT_EQ(target.query("sk_exp"), 0);

    g_user_list.erase(target.object_id());
    target.move(nullptr);
}

TEST_F(CmdWizardSystemTest, RecruitExp_SetQueryDelete)
{
    db->SetQueryFail(true);
    char no_record[256]  = "query no_such_master kid1/exp";
    g_notify_fail_msg[0] = 0;
    EXPECT_EQ(call_recruit_exp(me, 0, no_record), 0);
    EXPECT_STREQ(g_notify_fail_msg, "没有该玩家的收徒记录！");
    db->SetQueryFail(false);

    DbResultSet rows = CharMockDb::SingleRow({"kid1", "小徒", "100", "1"});
    db->SetResult(
        "select re_id, re_name, re_exp, re_apprentice from "
        "recruit_exp where re_master=wiz_master",
        rows);

    CMapping* ptr = g_channel.query_recruit("wiz_master");
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->query("kid1/exp"), 100) << "师傅记录由经验表装载";

    char set[256] = "set wiz_master kid1/exp 50";
    EXPECT_EQ(call_recruit_exp(me, 0, set), 1);
    EXPECT_EQ(ptr->query("kid1/exp"), 50) << "set 直接改写关系表";

    char query[256] = "query wiz_master kid1/exp";
    EXPECT_EQ(call_recruit_exp(me, 0, query), 1);

    char querystr[256] = "querystr wiz_master kid1/name";
    EXPECT_EQ(call_recruit_exp(me, 0, querystr), 1);

    char del[256] = "delete wiz_master kid1/exp";
    EXPECT_EQ(call_recruit_exp(me, 0, del), 1);
}

TEST_F(CmdWizardSystemTest, RewardExp_GrantsOneLevelWorth)
{
    StateRecordingUser target;
    target.set_name("受奖者", "wiz_reward");
    target.set("level", (long)10);
    target.set("combat_exp", (long)0);
    target.move(room);
    target.delete_temp("client/loading");
    g_user_list[target.object_id()] = &target;

    char arg[8] = "";
    EXPECT_EQ(call_reward_exp(me, target.object_id(), arg), 1);
    EXPECT_EQ(target.query("combat_exp"),
              CChar::level2_exp(11) - CChar::level2_exp(10));

    g_user_list.erase(target.object_id());
    target.move(nullptr);
}

TEST_F(CmdWizardSystemTest, SubExp_DropsOneLevel)
{
    StateRecordingUser target;
    target.set_name("受罚者", "wiz_sub");
    target.set("level", (long)10);
    target.set("combat_exp", (long)0);
    target.move(room);
    target.delete_temp("client/loading");
    g_user_list[target.object_id()] = &target;

    char arg[8] = "";
    EXPECT_EQ(call_sub_exp(me, target.object_id(), arg), 1);
    EXPECT_EQ(target.query("level"), 9);

    g_user_list.erase(target.object_id());
    target.move(nullptr);
}

// ---------------- 杂项系统命令 ----------------
TEST_F(CmdWizardSystemTest, Summon_DragsPlayerToWizardRoom)
{
    StateRecordingUser victim;
    victim.set_name("路人", "wiz_summoned");
    victim.set("grant", (long)0);
    victim.move(room);
    victim.delete_temp("client/loading");
    g_user_list[victim.object_id()] = &victim;

    char arg[8] = "";
    EXPECT_EQ(CallSummon(me, victim.object_id(), arg), 1);
    EXPECT_EQ(victim.environment(), me->environment()) << "人被拉到巫师身边";

    g_user_list.erase(victim.object_id());
    victim.move(nullptr);
}

TEST_F(CmdWizardSystemTest, Goto_JumpsToTargetRoom)
{
    CContainer* elsewhere = new CContainer();
    elsewhere->set("name", "wiz_elsewhere");
    elsewhere->set("wiz_room", (long)1);

    StateRecordingUser anchor;
    anchor.set_name("坐标", "wiz_anchor");
    anchor.move(elsewhere);
    anchor.delete_temp("client/loading");
    g_user_list[anchor.object_id()] = &anchor;

    char arg[256] = "";
    EXPECT_EQ(CallGoto(me, anchor.object_id(), arg), 1);
    EXPECT_EQ(me->environment(), elsewhere) << "goto 应跳到目标所在房间";

    me->move(nullptr);
    g_user_list.erase(anchor.object_id());
    anchor.move(nullptr);
    delete elsewhere;
}

TEST_F(CmdWizardSystemTest, ReloadLua_ScriptReportsResult)
{
    char missing[256] = "no_such_script_zzz";
    EXPECT_EQ(call_reload_lua(me, 0, missing), 1) << "报告失败也算处理完成";

    char one[256] = "enable_sample";
    EXPECT_EQ(call_reload_lua(me, 0, one), 1);
}

TEST_F(CmdWizardSystemTest, PrintMapping_ReturnsOk)
{
    char arg[8] = "";
    EXPECT_EQ(call_print_mapping(me, 0, arg), 1);
}

TEST_F(CmdWizardSystemTest, Update_FreshensEnvironment)
{
    char arg[8] = "";
    EXPECT_EQ(call_update(me, 0, arg), 1);
}

TEST_F(CmdWizardSystemTest, Save_AllPlayersPath)
{
    StateRecordingUser target;
    target.set_name("存档者", "wiz_saver");
    target.move(room);
    target.delete_temp("client/loading");
    g_user_list[target.object_id()] = &target;

    char all[8] = "all";
    EXPECT_EQ(CallSave(me, 0, all), 1);

    char none[8] = "";
    EXPECT_EQ(CallSave(me, 0, none), 1);

    g_user_list.erase(target.object_id());
    target.move(nullptr);
}

// ---------------- reload all 缺陷定位 ----------------

extern int lua_reload_script(const char* name);

extern int lua_reload_script(const char* name);

// 复现 reload all 的关键差异：清空 package.loaded 后再逐个重载

// 清空 package.loaded 后重建房间索引（reload_all_scripts 的最后一段）
TEST_F(CmdWizardSystemTest, Probe_LoadRoomIndexAfterClear)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L, "package.loaded = {}"), LUA_OK);

    CLuaEngine::instance()->load_room_index();
    EXPECT_NE(L, nullptr) << "引擎必须存活";
}

// reload all 必须能吞掉"脚本文件缺失"的负缓存条目：reload_script 读不到文件时
// 没有任何错误对象入栈，弹栈会让 Lua 栈失衡，后续 pcall 直接 PANIC（进程
// abort）
TEST_F(CmdWizardSystemTest, ReloadLua_AllPathSurvivesMissingDataScript)
{
    char all[256] = "all";
    EXPECT_EQ(call_reload_lua(me, 0, all), 1);
}

// ====================================================================
// systemparam 深路：字符串写入 / querystr / delete / 未知子命令
// ====================================================================
TEST_F(CmdWizardSystemTest, SysParam_StringSetQuerystrDeleteUnknown)
{
    // set 非数字值 → 字符串写入支(160-161)
    char setArg[512] = "set test_str_key hello";
    EXPECT_EQ(CallSysParam(me, 0, setArg), 1);
    EXPECT_STREQ(g_combat_config.querystr("test_str_key"), "hello")
        << "字符串参数写入可读回";

    // querystr 非 log_level → 字符串读出(198-202)
    char qsArg[512] = "querystr test_str_key";
    EXPECT_EQ(CallSysParam(me, 0, qsArg), 1);

    // delete 普通键(171-175)
    char delArg[512] = "delete test_str_key";
    EXPECT_EQ(CallSysParam(me, 0, delArg), 1);
    EXPECT_EQ(0, g_combat_config.query("test_str_key")) << "已删除";

    // 未知子命令 → 0(205)
    char badArg[512] = "frobnicate some_key";
    EXPECT_EQ(CallSysParam(me, 0, badArg), 0);
}

// ====================================================================
// call 深路：query_temp / querystr_temp 子命令(339-349)
// ====================================================================
TEST_F(CmdWizardSystemTest, Call_TempQueryBranches)
{
    char setNum[256] = "set_temp wiz_probe 5";
    EXPECT_EQ(call_wiz(me, 0, setNum), 1);

    char setStr[256] = "set_temp wiz_probe_str hello";
    EXPECT_EQ(call_wiz(me, 0, setStr), 1);

    char qNum[256] = "query_temp wiz_probe";
    EXPECT_EQ(call_wiz(me, 0, qNum), 1) << "query_temp 子命令(339-343)";

    char qStr[256] = "querystr_temp wiz_probe_str";
    EXPECT_EQ(call_wiz(me, 0, qStr), 1) << "querystr_temp 子命令(345-349)";

    room->delete_temp("wiz_probe");
    room->delete_temp("wiz_probe_str");
}

// ====================================================================
// call 深路：字符串字段写入(283-286) + 对用户 delete family(299-301)
// ====================================================================
TEST_F(CmdWizardSystemTest, Call_StringSetAndFamilyRelease)
{
    // 通用字符串字段（非 name/id）→ 283 else → who->set + update_me
    char descArg[256] = "set desc hello_desc";
    EXPECT_EQ(call_wiz(me, 0, descArg), 1);
    EXPECT_STREQ(room->querystr("desc"), "hello_desc")
        << "字符串字段写入(283-284)";

    // 目标为用户 → delete family 走 release_master(299-301)
    CUser* u = add_user();
    u->set("id", "wiz_fam_user");
    u->set_name("门徒", "wiz_fam_user");
    CMapping fam;
    fam.set("family_name", "测试门");
    fam.set("master_id", "someone");
    u->set("family", fam);
    u->move(room);
    u->delete_temp("client/loading");  // present() 可见

    char delFam[256] = "delete family";
    EXPECT_EQ(call_wiz(me, u->object_id(), delFam), 1)
        << "对用户删 family → release_master(299-301)";
    EXPECT_FALSE(u->querymap("family")) << "family 已整体删除";

    // 收尾：出房 + 出表 + 释放
    u->move(nullptr);
    remove_user(u->object_id());
    delete u;
    room->delete_temp("desc");
}

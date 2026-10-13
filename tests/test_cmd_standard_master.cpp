// test_cmd_standard_master.cpp - Unit tests for CmdStandardMaster.cpp
//
// Tests apprenticeship and teaching game state.
//
#include "pch.h"

#include "Char.h"
#include "User.h"
#include "Room.h"
#include "Skill.h"
#include "CmdStandard.h"
#include "LuaEngine.h"
#include "mock/mock_database.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

class CmdStandardMasterTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallApprentice(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"apprentice", me, target, arg);
    }
    static int CallRecruit(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"recruit", me, target, arg);
    }
    static int CallTeach(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"teach", me, target, arg);
    }
    static int CallLearn(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"learn", me, target, arg);
    }
    static int CallAbandon(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"abandon", me, target, arg);
    }
    static int CallEnable(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"enable", me, target, arg);
    }
    static int CallPrepare(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"prepare", me, target, arg);
    }

    static lua_State* HookL() { return CLuaEngine::instance()->get_state(); }

    // 组合判定所需技能面：test_a/test_b 未定义 valid_combine（默认0），
    // 分支必然落到 can_prepare_combine 判定
    static void MakeCombineSkills(CChar* c, long hubo = 0)
    {
        c->set_skill("test_a", 30);
        c->set_skill("test_b", 30);
        if (hubo > 0) c->set_skill("hubo", hubo);
        c->query_skill_map()->set("unarmed", "test_a");
        c->query_skill_map()->set("finger", "test_b");
    }

    static void RemoveCombineHook()
    {
        luaL_dostring(HookL(),
                      "__orig_ccp = hooks.can_prepare_combine "
                      "hooks.can_prepare_combine = nil");
    }

    static void RestoreCombineHook()
    {
        luaL_dostring(HookL(),
                      "hooks.can_prepare_combine = __orig_ccp "
                      "__orig_ccp = nil");
    }

    CChar* me;
    CChar* other;
    CContainer* room;

    void SetUp() override
    {
        ensure_lua_env();
        room = new CContainer();
        room->set("name", "test_room");
        room->set("infinity", 1);

        me = new CChar();
        me->set("name", "师父");
        me->set("id", "master1");
        me->set("level", (long)80);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->set("combat_exp", (long)5000000);
        me->set("recruit_exp", (long)100);
        me->set("family/family_name", "武当");
        me->set("family/master_id", "elder1");
        me->set("family/generation", (long)5);
        me->move(room);

        other = new CChar();
        other->set("name", "弟子");
        other->set("id", "disciple1");
        other->set("level", (long)30);
        other->set("hp", (long)100);
        other->set("combat_exp", (long)500000);
        other->set("family/family_name", "武当");
        other->move(room);
    }

    void TearDown() override
    {
        other->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete other;
        delete me;
        delete room;
    }
};

TEST_F(CmdStandardMasterTest, Apprentice_NoTarget)
{
    int result = CallApprentice(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Apprentice_Self)
{
    int result = CallApprentice(me, me->object_id(), (char*)"");
    // can't apprentice self
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Apprentice_ValidTarget)
{
    int result = CallApprentice(me, other->object_id(), (char*)"");
    // sets pending or succeeds
    EXPECT_NE(result, -1);
}

TEST_F(CmdStandardMasterTest, Recruit_NoTarget)
{
    int result = CallRecruit(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Recruit_Self)
{
    int result = CallRecruit(me, me->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Teach_NoArgs)
{
    int result = CallTeach(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Learn_NoArgs)
{
    int result = CallLearn(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Abandon_NoArgs)
{
    int result = CallAbandon(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Enable_NoArgs)
{
    int result = CallEnable(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardMasterTest, Prepare_NoArgs)
{
    int result = CallPrepare(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

// ---------------- can_prepare_combine 三态（两个调用点） ----------------
TEST_F(CmdStandardMasterTest, Prepare_TwoSkill_RealHookDeniesWithoutHubo)
{
    MakeCombineSkills(me);
    g_notify_fail_msg[0] = '\0';
    char cmd[]           = "test_a test_b";
    EXPECT_EQ(CallPrepare(me, 0, cmd), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这两种拳术技能不能组合。");
}

// 缺席时仅由 valid_combine 决定：即使 hubo 技能充足也不放行
TEST_F(CmdStandardMasterTest, Prepare_AbsentHook_IgnoresDatasetSkill)
{
    RemoveCombineHook();
    MakeCombineSkills(me, 100);
    g_notify_fail_msg[0] = '\0';
    char cmd[]           = "test_a test_b";
    EXPECT_EQ(CallPrepare(me, 0, cmd), 0);
    RestoreCombineHook();
    EXPECT_STREQ(g_notify_fail_msg, "这两种拳术技能不能组合。");
}

TEST_F(CmdStandardMasterTest, Prepare_SingleSkill_SiteTwoFalseIsFinal)
{
    MakeCombineSkills(me);
    char first[] = "test_a";
    ASSERT_EQ(CallPrepare(me, 0, first), 1);
    ASSERT_EQ(me->query_skill_prepare()->get_count(), 1);

    // 注入恒 false：能力判定以脚本返回为准
    luaL_dostring(HookL(),
                  "hooks.can_prepare_combine = function() return false end");
    g_notify_fail_msg[0] = '\0';
    char second[]        = "test_b";
    EXPECT_EQ(CallPrepare(me, 0, second), 0);
    RestoreCombineHook();
    EXPECT_STREQ(g_notify_fail_msg, "这种技能不能於你现有技能组合。");
}

// ---------------- enforce：内力分配 ----------------
static int call_enforce(CChar* me, int32_t target, char* arg)
{
    CCmdStandard cmd;
    return cmd.handlecommand((char*)"enforce", me, target, arg);
}

TEST_F(CmdStandardMasterTest, Enforce_WithoutArg_ShowsFormat)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_enforce(me, 0, (char*)""), 0);
    EXPECT_STREQ(g_notify_fail_msg, "指令格式：enforce <使出几成内力伤敌>");
}

TEST_F(CmdStandardMasterTest, Enforce_WithoutMappedForce_Rejected)
{
    char arg[8]          = "10";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_enforce(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你必须先 enable 一种内功。");
}

TEST_F(CmdStandardMasterTest, Enforce_RangeCheckedAgainstForceSkill)
{
    me->set_skill("force", 100);
    me->query_skill_map()->set("force", "other_gong");

    // 有效等级 = force/2 = 50，每击内力上限 25
    char too_big[8]      = "60";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_enforce(me, 0, too_big), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "你只能用0表示不运内力，或数字表示每一击用几点内力。");

    char negative[8]     = "-5";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(call_enforce(me, 0, negative), 0);

    char ok[8] = "20";
    EXPECT_EQ(call_enforce(me, 0, ok), 1);
    EXPECT_EQ(me->query("mp_factor"), 20);

    char zero[8] = "0";
    EXPECT_EQ(call_enforce(me, 0, zero), 1);
    EXPECT_EQ(me->query("mp_factor"), 0);
}

// ---------------- abandon：放弃技能 ----------------
TEST_F(CmdStandardMasterTest, Abandon_RequiresIdentityValidation)
{
    char arg[16]         = "unarmed";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAbandon(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "请先使用 </validate 你的密码 你的暗码> "
                 "命令验证你的身份后方可放弃你的技能。");
}

TEST_F(CmdStandardMasterTest, Abandon_RejectsEmptyAndUnknownSkill)
{
    me->set_temp("validate", 1);

    char empty[8]        = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAbandon(me, 0, empty), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你要放弃哪一项技能？");

    char unknown[24]     = "no_such_skill_zzz";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallAbandon(me, 0, unknown), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你并没有学过这项技能。");

    me->delete_temp("validate");
}

TEST_F(CmdStandardMasterTest, Abandon_DeletesSkillAndClearsItsMapping)
{
    me->set_temp("validate", 1);
    me->set_skill("unarmed", 50);
    me->set_skill("strike", 30);
    me->query_skill_map()->set("unarmed", "strike");

    char arg[16] = "strike";
    EXPECT_EQ(CallAbandon(me, 0, arg), 1);
    EXPECT_EQ(me->query_skill("strike", 1), 0);
    EXPECT_STREQ(me->query_skill_mapped("unarmed"), "");
    EXPECT_EQ(me->query_skill("unarmed", 1), 50) << "映射指向的技能不受影响";

    me->delete_temp("validate");
}

// ---------------- enable：技能激发 ----------------
TEST_F(CmdStandardMasterTest, Enable_ListsMappedSkills)
{
    me->set_skill("unarmed", 50);
    me->set_skill("enable_sample", 30);
    me->query_skill_map()->set("unarmed", "enable_sample");

    char arg[8] = "";
    EXPECT_EQ(CallEnable(me, 0, arg), 1);
}

TEST_F(CmdStandardMasterTest, Enable_UnknownTypeRejected)
{
    char arg[32]         = "nope enable_sample";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallEnable(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg,
                 "没有这个技能种类，用 enable ? 可以查看有哪些种类。");
}

TEST_F(CmdStandardMasterTest, Enable_UnlearnedSkillRejected)
{
    char arg[40]         = "unarmed no_such_skill_zzz";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallEnable(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你不会这种技能。");
}

TEST_F(CmdStandardMasterTest, Enable_ChangingForceResetsInnerPower)
{
    me->set_skill("enable_sample", 60);
    me->set("mp", (long)80);
    me->set("mp_factor", (long)12);

    char arg[32] = "force enable_sample";
    EXPECT_EQ(CallEnable(me, 0, arg), 1);
    EXPECT_STREQ(me->query_skill_mapped("force"), "enable_sample");
    EXPECT_EQ(me->query("mp"), 0) << "换内功必须清空内力";
    EXPECT_EQ(me->query("mp_factor"), 0);
}

TEST_F(CmdStandardMasterTest, Enable_ClearsPreparedCombo)
{
    me->set_skill("enable_sample", 60);
    me->query_skill_map()->set("unarmed", "old_skill");
    me->query_skill_prepare()->set("unarmed", "test_a");

    char arg[32] = "unarmed enable_sample";
    EXPECT_EQ(CallEnable(me, 0, arg), 1);
    EXPECT_EQ(me->query_skill_prepare()->get_count(), 0);
    EXPECT_STREQ(me->query_skill_mapped("unarmed"), "enable_sample");
}

TEST_F(CmdStandardMasterTest, Enable_NoneClearsMapping)
{
    me->set_skill("enable_sample", 60);
    me->query_skill_map()->set("unarmed", "enable_sample");

    char arg[24] = "unarmed none";
    EXPECT_EQ(CallEnable(me, 0, arg), 1);
    EXPECT_STREQ(me->query_skill_mapped("unarmed"), "");
}

// ---------------- prepare：空手组合 ----------------
TEST_F(CmdStandardMasterTest, Prepare_NoneWithoutCombo_Rejected)
{
    MakeCombineSkills(me);

    char arg[8]          = "none";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPrepare(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你现在没有组合任何特殊拳术技能。");
}

TEST_F(CmdStandardMasterTest, Prepare_NoneClearsExistingCombo)
{
    MakeCombineSkills(me);
    me->query_skill_prepare()->set("unarmed", "test_a");

    char arg[8] = "none";
    EXPECT_EQ(CallPrepare(me, 0, arg), 1);
    EXPECT_EQ(me->query_skill_prepare()->get_count(), 0);
}

TEST_F(CmdStandardMasterTest, Prepare_RejectsUnknownSkill)
{
    MakeCombineSkills(me);

    char arg[24]         = "no_such_skill_zzz";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPrepare(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你不会这种技能。");
}

TEST_F(CmdStandardMasterTest, Prepare_RejectsSkillNeverEnabled)
{
    MakeCombineSkills(me);
    me->set_skill("test_c", 30);

    char arg[16]         = "test_c";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPrepare(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "这种技能尚未激发或目前不能准备。");
}

TEST_F(CmdStandardMasterTest, Prepare_RejectsThirdCombo)
{
    MakeCombineSkills(me);
    me->query_skill_prepare()->set("unarmed", "test_a");
    me->query_skill_prepare()->set("finger", "test_b");

    char arg[16]         = "test_a";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallPrepare(me, 0, arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "你现在已经组合了两种特殊技能。");
}

// ---------------- apprentice / recruit：拜师收徒握手 ----------------
TEST_F(CmdStandardMasterTest, Apprentice_CompletesPendingRecruitSameFamily)
{
    other->set_temp("pending/recruit", me->object_id());

    char arg[8] = "";
    EXPECT_EQ(CallApprentice(me, other->object_id(), arg), 1);
    EXPECT_TRUE(me->is_apprentice_of(other)) << "握手成功即成师徒";
    EXPECT_STREQ(me->querystr("family/master_id"), "disciple1");
    EXPECT_EQ(other->query_temp("pending/recruit"), 0) << "待办已被消费";
}

// 本人无师门时不触发"改换门派"拦截，走另一条叩头文案
TEST_F(CmdStandardMasterTest, Apprentice_CompletesPendingRecruitWithoutFamily)
{
    me->del("family");
    other->set_temp("pending/recruit", me->object_id());

    char arg[8] = "";
    EXPECT_EQ(CallApprentice(me, other->object_id(), arg), 1);
    EXPECT_TRUE(me->is_apprentice_of(other));
    EXPECT_EQ(other->query_temp("pending/recruit"), 0);
}

TEST_F(CmdStandardMasterTest, Apprentice_AlreadyDisciple_ReturnsGreeting)
{
    me->set("family/master_id", "disciple1");

    char arg[8] = "";
    EXPECT_EQ(CallApprentice(me, other->object_id(), arg), 1);
    EXPECT_TRUE(me->is_apprentice_of(other));
}

TEST_F(CmdStandardMasterTest, Apprentice_UserWithoutRecruitExp_Rejected)
{
    StateRecordingUser newbie;
    newbie.set_name("新手", "brand_new");
    newbie.set("hp", (long)100);
    newbie.set("max_hp", (long)100);
    newbie.set("recruit_exp", (long)0);
    newbie.move(room);
    // 玩家进房会挂 client/loading 等客户端回执，未回执即对 present(id) 不可见
    newbie.delete_temp("client/loading");
    ASSERT_NE(room->present(newbie.object_id()), nullptr)
        << "新角色必须在房间中可见";

    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallApprentice(me, newbie.object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方还没有收徒资格！");

    newbie.move(nullptr);
}

TEST_F(CmdStandardMasterTest, Recruit_CompletesPendingApprentice)
{
    other->set_temp("pending/apprentice", me->object_id());

    char arg[8] = "";
    EXPECT_EQ(CallRecruit(me, other->object_id(), arg), 1);
    EXPECT_TRUE(other->is_apprentice_of(me)) << "握手成功即成师徒";
    EXPECT_EQ(other->query_temp("pending/apprentice"), 0) << "待办已被消费";
    EXPECT_EQ(other->query("family/generation"), 6);
}

TEST_F(CmdStandardMasterTest, Recruit_TargetAlreadyDisciple_ReturnsGreeting)
{
    other->set("family/family_name", "武当");
    other->set("family/master_id", "master1");

    char arg[8] = "";
    EXPECT_EQ(CallRecruit(me, other->object_id(), arg), 1);
    EXPECT_TRUE(other->is_apprentice_of(me));
}

TEST_F(CmdStandardMasterTest, Recruit_WithoutFamily_Rejected)
{
    me->del("family");

    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRecruit(me, other->object_id(), arg), 0);
    EXPECT_STREQ(
        g_notify_fail_msg,
        "你并不属于任何帮派，你必须先加入一个帮派，或自己创一个才能收徒。");
}

TEST_F(CmdStandardMasterTest, Recruit_TargetFromAnotherFamily_Rejected)
{
    other->set("family/family_name", "少林");

    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallRecruit(me, other->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "未经其师尊同意就挖人家墙角不太好吧。");
}

// ---------------- expell：只能开革玩家 ----------------
TEST_F(CmdStandardMasterTest, Expell_NonUserTarget_Rejected)
{
    CCmdStandard cmd;
    char arg[8]          = "";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(cmd.handlecommand((char*)"expell", me, other->object_id(), arg),
              0);
    EXPECT_STREQ(g_notify_fail_msg, "你只能开革玩家所扮演的人物。");
}

// =====================================================================
// 深度路径：师徒/伴侣/练功的成功链路与未覆盖守卫
// （录制师父接 tell/菜单；徒弟为普通角色，状态变化即观察点）
// =====================================================================
class CmdMasterDeepTest : public ::testing::Test {
protected:
    StateRecordingUser* master;  // 师父（录制，接 tell/菜单文案）
    CChar* student;              // 徒弟（普通角色）
    CContainer* room;
    MockDatabase* db = nullptr;  // expell 等路径会落库（g_db 空引用即崩）

    static int Call(CChar* who, const char* name, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand(const_cast<char*>(name), who, target, arg);
    }

    static lua_State* HookL() { return CLuaEngine::instance()->get_state(); }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        g_notify_fail_msg[0] = '\0';
        db                   = new MockDatabase();
        g_db                 = db;
        // g_item_mgr 依赖 test_env 全局环境安装的共享 g_testItemMgr：
        // 本套件不换入（set_title→update_me→downloadobject→find_object
        // 的隐式路径直接消费共享管理器，链路被谁破坏就让谁暴露）
        room = new CContainer();
        room->set_name("讲武堂", "master_deep_room");
        room->set("infinity", 1);

        master = new StateRecordingUser();
        master->set_name("师父", "master1");
        master->set("level", (long)80);
        master->set("hp", (long)500);
        master->set("max_hp", (long)500);
        master->set("combat_exp", (long)5000000);
        master->set("recruit_exp", (long)100);
        master->set("family/family_name", "武当");
        master->set("family/master_id", "elder1");
        master->set("family/generation", (long)5);
        master->move(room);
        master->delete_temp("client/loading");

        student = new CChar();
        student->set_name("弟子", "disciple1");
        student->set("level", (long)30);
        student->set("hp", (long)500);
        student->set("max_hp", (long)500);
        student->set("combat_exp", (long)500000);
        student->set("family/family_name", "武当");
        student->move(room);
    }

    void TearDown() override
    {
        student->move(nullptr);
        delete student;
        master->move(nullptr);
        g_user_list.erase(master->object_id());
        delete master;
        room->move(nullptr);
        delete room;
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    // 建立"master 是 student 的师父"关系（is_apprentice_of 的判定面）
    void MakeMasterOf() { master->set("family/master_id", "disciple1"); }
};

// ---------------- apprentice 守卫与玩家目标通知 ----------------

TEST_F(CmdMasterDeepTest, Apprentice_NoApprenticeRoom_Rejected)
{
    room->set("no_apprentice", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "apprentice", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止拜师"));
}

TEST_F(CmdMasterDeepTest, Apprentice_NotLiving_Rejected)
{
    student->set_temp("disabled", "1");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "apprentice", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("弄醒"));
}

TEST_F(CmdMasterDeepTest, Apprentice_DifferentFamily_Rejected)
{
    student->set("family/family_name", "峨眉");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "apprentice", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("改换门派"));
}

TEST_F(CmdMasterDeepTest, Apprentice_OwnDisciple_Rejected)
{
    student->set("family/master_id", "master1");  // 对方已是我的徒弟
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "apprentice", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("拜自己的徒弟"));
}

TEST_F(CmdMasterDeepTest, Apprentice_ToUserTarget_NotifiesBothSides)
{
    auto* user = new StateRecordingUser();
    user->set_name("玩家", "apprent_target");
    user->set("recruit_exp", (long)100);      // 收徒资格守卫要求玩家有资格
    user->set("family/family_name", "武当");  // 同门守卫：改换门派被拒
    user->move(room);
    user->delete_temp("client/loading");
    g_user_list[user->object_id()] = user;

    EXPECT_EQ(1, Call(master, "apprentice", user->object_id(), (char*)""));
    EXPECT_TRUE(user->has("如果你愿意收")) << "玩家目标收到收徒询问";
    EXPECT_TRUE(master->has("你想要拜")) << "发起方收到意向回执";
    EXPECT_EQ(user->object_id(), master->query_temp("pending/apprentice"))
        << "挂起拜师意向";

    user->move(nullptr);
    g_user_list.erase(user->object_id());
    delete user;
}

// ---------------- recruit：守卫、提议、收徒落地 ----------------

TEST_F(CmdMasterDeepTest, Recruit_NoRecruitRoom_Rejected)
{
    room->set("no_recruit", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "recruit", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止收徒"));
}

TEST_F(CmdMasterDeepTest, Recruit_NoQualification_Rejected)
{
    master->set("recruit_exp", (long)0);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "recruit", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("收徒资格"));
}

TEST_F(CmdMasterDeepTest, Recruit_Proposal_SetsPending)
{
    auto* user = new StateRecordingUser();
    user->set_name("玩家", "recruit_proposal");
    user->move(room);
    user->delete_temp("client/loading");
    g_user_list[user->object_id()] = user;

    EXPECT_EQ(1, Call(master, "recruit", user->object_id(), (char*)""));
    EXPECT_EQ(user->object_id(), master->query_temp("pending/recruit"))
        << "挂起收徒意向";
    EXPECT_TRUE(user->has("如果你愿意拜")) << "目标收到拜师邀请";

    user->move(nullptr);
    g_user_list.erase(user->object_id());
    delete user;
}

TEST_F(CmdMasterDeepTest, Recruit_AcceptEstablishesFamily)
{
    // 双向意向齐备：recruit 直接落地收徒
    // 注意：set_title→update_me 会对房内玩家回放 "downloadobject <id>"
    // 指令，find_object 消费 test_env 安装的共享 g_testItemMgr。
    student->set_temp("pending/apprentice", master->object_id());
    EXPECT_EQ(1, Call(master, "recruit", student->object_id(), (char*)""));
    EXPECT_TRUE(master->has("恭喜你新收了一名弟子"));
    EXPECT_STREQ("master1", student->querystr("family/master_id"))
        << "徒弟记下师父账号";
    EXPECT_EQ(6, student->query("family/generation")) << "世代顺延";
    EXPECT_EQ('\0', student->querystr_temp("pending/apprentice")[0])
        << "意向清空";
}

// ---------------- expell：特权开革与师徒开革 ----------------

TEST_F(CmdMasterDeepTest, Expell_NoExpellRoom_Rejected)
{
    room->set("no_expell", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "expell", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止弃徒"));
}

TEST_F(CmdMasterDeepTest, Expell_PrivilegedExpellsUser)
{
    auto* victim = new StateRecordingUser();
    victim->set_name("门徒", "expell_vic");
    victim->set("family/family_name", "武当");
    victim->set_skill("test_a", 30);
    victim->move(room);
    victim->delete_temp("client/loading");
    g_user_list[victim->object_id()] = victim;

    master->set("family/privs", (long)-1);  // 掌门特权
    EXPECT_EQ(1, Call(master, "expell", victim->object_id(), (char*)""));
    EXPECT_TRUE(victim->has("你被开革出师门了"));
    EXPECT_EQ(nullptr, victim->querymap("family")) << "师门档案清空";
    // 先 del("family") 再拼称号：门派名已空，落成"弃徒"
    EXPECT_STREQ("弃徒", victim->querystr("title"));
    EXPECT_EQ(15, victim->query_skill("test_a", 1)) << "开革后技能减半";

    victim->move(nullptr);
    g_user_list.erase(victim->object_id());
    delete victim;
}

TEST_F(CmdMasterDeepTest, Expell_MasterExpellsApprentice)
{
    auto* victim = new StateRecordingUser();
    victim->set_name("门徒", "expell_app");
    victim->set("family/family_name", "武当");
    victim->set("family/master_id", "master1");
    victim->move(room);
    victim->delete_temp("client/loading");
    g_user_list[victim->object_id()] = victim;

    EXPECT_EQ(1, Call(master, "expell", victim->object_id(), (char*)""));
    EXPECT_TRUE(victim->has("你被师父开革出师门了"));
    EXPECT_EQ(nullptr, victim->querymap("family"));

    victim->move(nullptr);
    g_user_list.erase(victim->object_id());
    delete victim;
}

// ---------------- teach：伴侣传授 ----------------

TEST_F(CmdMasterDeepTest, Teach_BusyMaster_Rejected)
{
    master->set("couple/id", "disciple1");
    master->start_busy(1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("正忙着呢"));
}

TEST_F(CmdMasterDeepTest, Teach_SpouseMissing_Rejected)
{
    master->set("couple/id", "spouse_absent");
    master->set("couple/gender", "妻子");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不在这里"));
}

TEST_F(CmdMasterDeepTest, Teach_StudentBusy_Rejected)
{
    master->set("couple/id", "disciple1");
    student->start_busy(1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("正忙着呢"));
}

TEST_F(CmdMasterDeepTest, Teach_ListsSkillsWithoutArg)
{
    master->set("couple/id", "disciple1");
    master->set_skill("test_a", 30);

    EXPECT_EQ(1, Call(master, "teach", 0, (char*)""));
    EXPECT_TRUE(master->has("离开")) << "技能菜单以离开项收尾";
}

TEST_F(CmdMasterDeepTest, Teach_StudentOutmatches_Rejected)
{
    master->set("couple/id", "disciple1");
    master->set_skill("test_a", 30);
    student->set_skill("test_a", 50);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不输于你"));
}

TEST_F(CmdMasterDeepTest, Teach_MasterTooTired_ReturnsWithoutTeaching)
{
    master->set("couple/id", "disciple1");
    master->set_skill("test_a", 30);
    student->set_skill("test_a", 5);
    master->set("hp", (long)50);  // <= gin_cost(100)
    EXPECT_EQ(1, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_TRUE(master->has("你太累了")) << "师父疲劳时收手";
    // query_skill 无 raw 参数返回 raw/2：技能等级看 raw 面
    EXPECT_EQ(5, student->query_skill("test_a", 1)) << "没教成不涨技能";
}

TEST_F(CmdMasterDeepTest, Teach_Success_TransfersSkill)
{
    master->set("couple/id", "disciple1");
    master->set_skill("test_a", 30);
    student->set_skill("test_a", 5);

    EXPECT_EQ(1, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_TRUE(master->has("解释有关")) << "师父收到讲解回执";
    EXPECT_EQ(400, master->query("hp")) << "传授扣师父 100 hp";
    EXPECT_EQ(400, student->query("hp")) << "学艺扣徒弟 100 hp";
    EXPECT_GT(student->query_skill("test_a", 1), 5) << "徒弟技能提升";
    EXPECT_GT(student->query("sk_exp"), 0) << "徒弟记技能经验";
}

// ---------------- learn：出师请教 ----------------

TEST_F(CmdMasterDeepTest, Learn_NoMasterRelation_Rejected)
{
    student->set_skill("test_a", 30);
    char arg[]           = "test_a";
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), arg));
    EXPECT_TRUE(master->has("您见笑了")) << "非师徒被婉拒";
}

TEST_F(CmdMasterDeepTest, Learn_NoArgWithTarget_Rejected)
{
    MakeMasterOf();
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("指令格式"));
}

TEST_F(CmdMasterDeepTest, Learn_TeacherNotLiving_Rejected)
{
    MakeMasterOf();
    student->set_temp("disabled", "1");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), (char*)"test_a"));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("弄醒"));
}

TEST_F(CmdMasterDeepTest, Learn_TeacherWithoutSkill_Rejected)
{
    MakeMasterOf();
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("找别人学"));
}

TEST_F(CmdMasterDeepTest, Learn_TeacherRefusesEnv_Rejected)
{
    MakeMasterOf();
    student->set_skill("test_a", 30);
    student->set("env/no_teach", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), (char*)"test_a"));
    EXPECT_TRUE(master->has("以后再说吧")) << "师傅闭口不谈";
}

TEST_F(CmdMasterDeepTest, Learn_FromPlayerTeacher_DrainsTeacherHp)
{
    // 师父是玩家时走耗损分支：教师掉 100 hp，学生长技能
    auto* teacher = new StateRecordingUser();
    teacher->set_name("同门", "teacher1");
    teacher->set("family/family_name", "武当");
    teacher->set("level", (long)80);
    teacher->set("hp", (long)500);
    teacher->set("combat_exp", (long)5000000);
    teacher->set_skill("test_a", 30);
    teacher->move(room);
    teacher->delete_temp("client/loading");
    g_user_list[teacher->object_id()] = teacher;

    master->set("family/master_id", "teacher1");  // master 是 teacher 的徒弟
    master->set_skill("test_a", 5);

    EXPECT_EQ(1, Call(master, "learn", teacher->object_id(), (char*)"test_a"));
    EXPECT_EQ(400, teacher->query("hp")) << "玩家师父授艺掉 100 hp";
    EXPECT_EQ(400, master->query("hp")) << "学生请教掉 100 hp";
    EXPECT_GT(master->query_skill("test_a", 1), 5) << "技能提升";
    EXPECT_GT(master->query("sk_exp"), 0) << "记技能经验";

    teacher->move(nullptr);
    g_user_list.erase(teacher->object_id());
    delete teacher;
}

TEST_F(CmdMasterDeepTest, Learn_TeacherTooTired_Returns)
{
    auto* teacher = new StateRecordingUser();
    teacher->set_name("同门", "teacher_tired");
    teacher->set("family/family_name", "武当");
    teacher->set("level", (long)80);
    teacher->set("hp", (long)300);  // <= 350：太累
    teacher->set("combat_exp", (long)5000000);
    teacher->set_skill("test_a", 30);
    teacher->move(room);
    teacher->delete_temp("client/loading");
    g_user_list[teacher->object_id()] = teacher;

    master->set("family/master_id", "teacher_tired");
    master->set_skill("test_a", 5);

    EXPECT_EQ(1, Call(master, "learn", teacher->object_id(), (char*)"test_a"));
    // say(msg, me) 投递给求教方（me）
    EXPECT_TRUE(master->has("太累了")) << "师父太累改天再教";
    EXPECT_EQ(5, master->query_skill("test_a", 1)) << "没学成不涨技能";

    teacher->move(nullptr);
    g_user_list.erase(teacher->object_id());
    delete teacher;
}

// ---------------- enable：遗留分支 ----------------

TEST_F(CmdMasterDeepTest, Enable_CheckUnknownTarget_Rejected)
{
    master->set("grant", (long)CChar::GRANT_WIZARD);
    g_notify_fail_msg[0] = '\0';
    char arg[]           = "check";
    EXPECT_EQ(0, Call(master, "enable", 999999, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("没有这个人"));
}

TEST_F(CmdMasterDeepTest, Enable_NoMapTo_ShowsFormat)
{
    g_notify_fail_msg[0] = '\0';
    char arg[]           = "unarmed";
    EXPECT_EQ(0, Call(master, "enable", 0, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("指令格式"));
}

TEST_F(CmdMasterDeepTest, Enable_BaseSkill_NotNeeded)
{
    master->set_skill("unarmed", 30);
    char arg[] = "unarmed unarmed";
    EXPECT_EQ(1, Call(master, "enable", 0, arg));
    EXPECT_TRUE(master->has("是基础")) << "基础技能无须 enable";
}

TEST_F(CmdMasterDeepTest, Enable_IncompatibleUsage_Rejected)
{
    master->set_skill("test_a", 30);
    g_notify_fail_msg[0] = '\0';
    char arg[]           = "unarmed test_a";
    EXPECT_EQ(0, Call(master, "enable", 0, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能当成这种用途"));
}

// ---------------- prepare：钩子放行的双技组合成功路径 ----------------

TEST_F(CmdMasterDeepTest, Prepare_TwoSkill_HookAllows)
{
    master->set_skill("test_a", 30);
    master->set_skill("test_b", 30);
    master->query_skill_map()->set("unarmed", "test_a");
    master->query_skill_map()->set("finger", "test_b");

    luaL_dostring(HookL(),
                  "hooks.can_prepare_combine = function() return true end");
    char cmd[] = "test_a test_b";
    int rc     = Call(master, "prepare", 0, cmd);
    luaL_dostring(HookL(), "hooks.can_prepare_combine = nil");

    EXPECT_EQ(1, rc);
    EXPECT_EQ(2, master->query_skill_prepare()->get_count())
        << "两项拳术都已激发";
    EXPECT_TRUE(master->has("完成技能准备"));
}

// ---------------- study：研读兵书 ----------------

namespace {
// 造一本含技能研读数据的书（study 读取物件上的 skill 映射）
CContainer* MakeStudyBook(CChar* who, const char* skill_name,
                          long max_skill = 400)
{
    CItem* book = new CItem();
    book->set_name("无名剑谱", "study_book");
    CMapping sk;
    sk.set("name", skill_name);
    sk.set("exp_required", 0L);
    sk.set("difficulty", 0L);
    sk.set("jing_cost", 10L);
    sk.set("max_skill", max_skill);
    sk.set("min_skill", 0L);
    book->set("skill", sk);
    book->move(who);
    return book;
}
}  // namespace

TEST_F(CmdMasterDeepTest, Study_NoBook_Rejected)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", 123456, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你要读什么"));
}

TEST_F(CmdMasterDeepTest, Study_BookWithoutSkill_Rejected)
{
    CItem* plain = new CItem();
    plain->set_name("闲书", "plain_book");
    plain->move(master);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", plain->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("无法从这样东西学到"));
    plain->move(nullptr);
    delete plain;
}

TEST_F(CmdMasterDeepTest, Study_Illiterate_Rejected)
{
    CContainer* book = MakeStudyBook(master, "test_a");
    master->set_skill("test_a", 30);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", book->object_id(), (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("文盲"));
    book->move(nullptr);
    delete book;
}

TEST_F(CmdMasterDeepTest, Study_Success_ImprovesSkill)
{
    CContainer* book = MakeStudyBook(master, "test_a");
    master->set_skill("test_a", 30);
    master->set_skill("literate", 10);

    EXPECT_EQ(1, Call(master, "study", book->object_id(), (char*)""));
    EXPECT_TRUE(master->has("似乎有点心得"));
    EXPECT_LT(master->query("hp"), 500) << "研读耗神";
    book->move(nullptr);
    delete book;
}

TEST_F(CmdMasterDeepTest, Study_TooTired_ReturnsZero)
{
    CContainer* book = MakeStudyBook(master, "test_a");
    master->set_skill("test_a", 30);
    master->set_skill("literate", 10);
    master->set("hp", (long)3);  // 低于 jing_cost(10)

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", book->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("过于疲倦"));
    EXPECT_EQ(0, master->query("hp")) << "勉强耗尽心神";
    book->move(nullptr);
    delete book;
}

TEST_F(CmdMasterDeepTest, Study_MartialCap_NotLearned)
{
    CContainer* book = MakeStudyBook(master, "test_a", 2000);
    master->set_skill("test_a", 300);  // >= level2skill(80)=276：触顶
    master->set_skill("literate", 10);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", book->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("无法领会"))
        << "martial 技能受等级上限约束";
    book->move(nullptr);
    delete book;
}

// ---------------- practice：练功 ----------------

TEST_F(CmdMasterDeepTest, Practice_NoPracticeRoom_Rejected)
{
    room->set("no_practice", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "practice", 0, (char*)"unarmed"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止练功"));
}

TEST_F(CmdMasterDeepTest, Practice_NoArg_DelegatesToEnable)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "practice", 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("没有使用任何特殊技能"));
}

TEST_F(CmdMasterDeepTest, Practice_NotMapped_Rejected)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "practice", 0, (char*)"unarmed"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("enable 指定"));
}

TEST_F(CmdMasterDeepTest, Practice_SkillNotLearned_Rejected)
{
    master->query_skill_map()->set("unarmed", "test_a");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "practice", 0, (char*)"unarmed"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("还没「学会」"));
}

TEST_F(CmdMasterDeepTest, Practice_BasicNotLearned_Rejected)
{
    master->query_skill_map()->set("unarmed", "test_a");
    master->set_skill("test_a", 10);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "practice", 0, (char*)"unarmed"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("一窍不通"));
}

TEST_F(CmdMasterDeepTest, Practice_Success_ImprovesSkill)
{
    master->query_skill_map()->set("unarmed", "test_a");
    master->set_skill("unarmed", 3);
    master->set_skill("test_a", 10);

    EXPECT_EQ(1, Call(master, "practice", 0, (char*)"unarmed"));
    EXPECT_TRUE(master->has("进步了")) << "练习成功的回执";
}

// ---------------- 房间禁令/忙碌类守卫（teach/learn/study 首道闸） --------

TEST_F(CmdMasterDeepTest, Teach_NoTeachRoom_Rejected)
{
    room->set("no_teach", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "teach", 0, (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止传授技能"));
}

TEST_F(CmdMasterDeepTest, Learn_NoLearnRoom_Rejected)
{
    room->set("no_learn", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "learn", student->object_id(), (char*)"test_a"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止学习技能"));
}

TEST_F(CmdMasterDeepTest, Study_NoStudyRoom_Rejected)
{
    room->set("no_study", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", 123456, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止练功"));
}

TEST_F(CmdMasterDeepTest, Study_Busy_Rejected)
{
    master->start_busy(1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, Call(master, "study", 123456, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你现在正忙着呢"));
}

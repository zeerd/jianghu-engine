// test_setup_human.cpp — setup_human 各最小功能集的分支测试
//
// 契约：setup_human 在拆分前后行为不变，且每个功能集遵守自己的规则：
//   1) 未定义的先天属性按区间随机缺省，已定义的不被覆盖；
//   2) 最大气血按年龄阶梯公式计算，并受 userp/undefinedp 门控；
//   3) 内功气血加成受 taoism>50、qi_age>0、全真教覆盖（xiantian_gong>0 且
//      family=全真教 双条件）与 high_force_max_effect（取最大/求和，config.ini
//      [server]）控制；
//   4) NPC 的 max_mp/mp/unit/attitude 只在未定义时补缺省；
//   5) 玩家的掌门称号与等级缺省；头像矩阵由 hooks.setup_human 负责，
//      期望头像取自 Lua 规范常量（data/constants.lua）；
//   6) 体重只在为 0 时按 str 公式补齐。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "GameInit.h"
#include "Global.h"
#include "IniParser.h"
#include "LuaEngine.h"
#include "LuaHook.h"
#include "test_lua_env.h"
#include "test_scoped_room.h"
#include "Room.h"
#include "RoomMgr.h"
#include "ServerConfig.h"
#include "User.h"
#include "Utils.h"

namespace {

class SetupHumanTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        ApplyForceAddHp(false);
        m_saved_high_force = ini_get("server", "high_force_max_effect", "1");
        g_ini_data["server"]["high_force_max_effect"] = "1";
    }

    void TearDown() override
    {
        ApplyForceAddHp(false);
        g_ini_data["server"]["high_force_max_effect"] = m_saved_high_force;
    }

    static void ApplyForceAddHp(bool on)
    {
        ini_load("tests/test_config.ini");
        g_ini_data["server"]["force_add_hp"] = on ? "1" : "0";
        CServerConfig::init();
    }

    // 六项先天属性与等级固定：hp = con = 20，base = 300 + 10*20*3 = 900
    static CChar* MakeNpc(long age)
    {
        CChar* me = new CChar();
        me->set("id", "hum1");
        me->set("name", "测试角色");
        me->set("age", age);
        me->set("str", (long)20);
        me->set("con", (long)20);
        me->set("int", (long)20);
        me->set("dex", (long)20);
        me->set("per", (long)20);
        me->set("level", (long)10);
        return me;
    }

    static CUser* MakeUser(long age, bool set_level = true)
    {
        CUser* me = new CUser();
        me->set("id", "humu1");
        me->set("name", "测试玩家");
        me->set("age", age);
        me->set("str", (long)20);
        me->set("con", (long)20);
        me->set("int", (long)20);
        me->set("dex", (long)20);
        me->set("per", (long)20);
        if (set_level) me->set("level", (long)10);
        return me;
    }

    // 期望头像取自 Lua 规范常量；常量缺失即断言失败（同时守护常量装载链）
    static long LuaIcon(const char* name)
    {
        lua_State* L = CLuaEngine::instance()->get_state();
        EXPECT_NE(L, nullptr);
        lua_getglobal(L, name);
        EXPECT_TRUE(lua_isinteger(L, -1)) << "Lua 头像常量缺失: " << name;
        long v = lua_isinteger(L, -1) ? (long)lua_tointeger(L, -1) : -1;
        lua_pop(L, 1);
        return v;
    }

    std::string m_saved_high_force = "1";
};

}  // namespace

// ---------------------------------------------------------------------------
// 1) 随机属性缺省：未定义 → 落区间；已定义 → 不覆盖
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, BaseAttrs_Undefined_FillsRandomRanges)
{
    CChar* me = new CChar();
    setup_human(me);

    EXPECT_GE(me->query("age"), 15);
    EXPECT_LE(me->query("age"), 44);
    for (const char* key : {"str", "con", "int", "dex", "per"}) {
        EXPECT_GE(me->query(key), 10) << key;
        EXPECT_LE(me->query(key), 30) << key;
    }
    delete me;
}

TEST_F(SetupHumanTest, BaseAttrs_Defined_NotOverwritten)
{
    CChar* me = MakeNpc(30);
    setup_human(me);

    EXPECT_EQ(me->query("age"), 30);
    EXPECT_EQ(me->query("str"), 20);
    EXPECT_EQ(me->query("con"), 20);
    EXPECT_EQ(me->query("int"), 20);
    EXPECT_EQ(me->query("dex"), 20);
    EXPECT_EQ(me->query("per"), 20);
    delete me;
}

// ---------------------------------------------------------------------------
// 2) 最大气血：年龄阶梯 8 段（con=20, level=10, 无内功加成）
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, MaxHp_AgeLadder_MatchesFormula)
{
    struct Case {
        long age;
        long expected;
    };
    const Case cases[] = {
        {10, 900},    // age <= 14
        {30, 1220},   // age <= 40  : 900 + 16*20
        {60, 1726},   // age <= 100 : 900 + 60 + 46*20*5/6
        {120, 2836},  // age <= 150 : 900 + 240 + 106*20*4/5
        {160, 3570},  // age <= 200 : 900 + 480 + 146*20*3/4
        {210, 4353},  // age <= 250 : 900 + 840 + 196*20*2/3
        {260, 4660},  // age <= 300 : 900 + 1300 + 246*20/2
        {310, 4592},  // age >  300 : 900 + 3100 + 296*20/10
    };

    for (const auto& c : cases) {
        CChar* me = MakeNpc(c.age);
        setup_human(me);
        EXPECT_EQ(me->query("max_hp"), c.expected) << "age=" << c.age;
        delete me;
    }
}

TEST_F(SetupHumanTest, MaxHp_NpcDefined_SkipsRecompute)
{
    CChar* me = MakeNpc(30);
    me->set("max_hp", (long)999);
    setup_human(me);

    EXPECT_EQ(me->query("max_hp"), 999) << "NPC 已有 max_hp 不应重算";
    delete me;
}

TEST_F(SetupHumanTest, MaxHp_User_AlwaysRecomputes)
{
    CUser* me = MakeUser(30);
    me->set("max_hp", (long)999);
    setup_human(me);

    // userp 分支无视 undefinedp：900 + (30-14)*20 = 1220
    EXPECT_EQ(me->query("max_hp"), 1220) << "玩家每次 setup 都重算最大气血";
    delete me;
}

TEST_F(SetupHumanTest, MaxHp_AddHpFruit_BonusApplied)
{
    CChar* me = MakeNpc(30);
    me->set("add_hp", (long)500);
    setup_human(me);

    EXPECT_EQ(me->query("max_hp"), 1720) << "1220 + 珍果 500";
    delete me;
}

TEST_F(SetupHumanTest, MaxHp_BelowFloor_RaisedToOne)
{
    // con = -100 → base = 300 + 10*(-100)*3 = -2700（age<=14 不再加成）
    CChar* me = MakeNpc(10);
    me->set("con", (long)-100);
    setup_human(me);

    EXPECT_EQ(me->query("max_hp"), 1) << "max_hp < 1 时应抬到 1";
    delete me;
}

TEST_F(SetupHumanTest, MaxHp_TempSyncedToDbase)
{
    CChar* me = MakeNpc(30);
    setup_human(me);

    EXPECT_EQ(me->query_temp("max_hp"), me->query("max_hp"));
    delete me;
}

// ---------------------------------------------------------------------------
// 3) 内功气血加成
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, ForceBonus_TaoismGateClosed_NoBonus)
{
    struct Case {
        int taoism;
        const char* label;
    };
    const Case cases[] = {
        {50, "taoism=50 不过 >50 门"},
        {51, "taoism=51 但 qi_age=25-30<0"},
    };

    for (const auto& c : cases) {
        CChar* me = MakeNpc(30);
        me->set_skill("taoism", c.taoism);
        me->set_skill("taiji_shengong", 60);
        setup_human(me);

        EXPECT_EQ(me->query("max_hp"), 1220) << c.label;
        delete me;
    }
}

TEST_F(SetupHumanTest, ForceBonus_TaoismGateOpen_NoSkills_NoBonus)
{
    // taoism 只是门、本身不直接加分：三项技能全 0 时加成仍为 0
    CChar* me = MakeNpc(30);
    me->set("family/family_name", "全真教");
    me->set_skill("taoism", 100);
    setup_human(me);

    EXPECT_EQ(me->query("max_hp"), 1220);
    delete me;
}

TEST_F(SetupHumanTest, ForceBonus_XiantianBlockedWithoutTaoismGate)
{
    // xiantian_gong 单修、taoism 未过门 → 零加成（加成必须整体过 taoism 门）
    CChar* me = MakeNpc(30);
    me->set_skill("xiantian_gong", 90);
    setup_human(me);

    EXPECT_EQ(me->query("max_hp"), 1220);
    delete me;
}

// ---------------------------------------------------------------------------
// 4) NPC 的 MP / unit / attitude
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, NpcSetup_MaxMpUndefined_DerivedFromForce)
{
    // query_skill("force") 非 raw = skills/2 = 50 → max_mp = 1000
    CChar* me = MakeNpc(30);
    me->set_skill("force", 100);
    setup_human(me);

    EXPECT_EQ(me->query("max_mp"), 1000);
    EXPECT_EQ(me->query("mp"), 2000) << "mp = max_mp*2";
    EXPECT_STREQ(me->querystr("unit"), "位");
    EXPECT_STREQ(me->querystr("attitude"), "peaceful");
    delete me;
}

TEST_F(SetupHumanTest, NpcSetup_MaxMpDefined_KeptAndMpDoubled)
{
    CChar* me = MakeNpc(30);
    me->set("max_mp", (long)500);
    setup_human(me);

    EXPECT_EQ(me->query("max_mp"), 500) << "已有 max_mp 不被推导覆盖";
    EXPECT_EQ(me->query("mp"), 1000);
    delete me;
}

TEST_F(SetupHumanTest, NpcSetup_UnitAttitudeDefined_NotOverwritten)
{
    CChar* me = MakeNpc(30);
    me->set("unit", "头");
    me->set("attitude", "aggressive");
    setup_human(me);

    EXPECT_STREQ(me->querystr("unit"), "头");
    EXPECT_STREQ(me->querystr("attitude"), "aggressive");
    delete me;
}

// ---------------------------------------------------------------------------
// 6) 玩家侧：掌门称号 / 等级缺省 / 头像矩阵
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, UserSetup_LevelUndefined_DefaultsToOne)
{
    CUser* me = MakeUser(30, /*set_level=*/false);
    setup_human(me);

    EXPECT_EQ(me->query("level"), 1);
    delete me;
}

TEST_F(SetupHumanTest, UserSetup_IconDefinedInRandomClass_NotOverwritten)
{
    CUser* me = MakeUser(30);
    me->set("gender", "男性");
    me->set("icon", (long)777);  // class 未设 → 走随机分支的 undefinedp 检查
    setup_human(me);

    EXPECT_EQ(me->query("icon"), 777);
    delete me;
}

// 头像矩阵只作用于玩家：NPC 的 icon 不被改写，也不被补
TEST_F(SetupHumanTest, Icon_NpcBranchNotTouched)
{
    CChar* preset = MakeNpc(30);
    preset->set("class", "bonze");
    preset->set("icon", (long)777);
    setup_human(preset);
    EXPECT_EQ(preset->query("icon"), 777) << "NPC 预设头像不应被覆盖";
    delete preset;

    CChar* fresh = MakeNpc(30);
    fresh->set("class", "bonze");
    setup_human(fresh);
    EXPECT_EQ(fresh->query("icon"), 0) << "NPC 头像不在钩子职责内";
    delete fresh;
}

// 钩子缺席时玩家头像保持未定义
TEST_F(SetupHumanTest, Icon_HookAbsent_LeavesIconUndefined)
{
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_fb = hooks.setup_human "
                            "hooks.setup_human = nil"),
              LUA_OK);

    CUser* me = MakeUser(30);
    me->set("gender", "男性");
    setup_human(me);

    luaL_dostring(L, "hooks.setup_human = __orig_fb __orig_fb = nil");
    EXPECT_EQ(me->query("icon"), 0) << "钩子缺席时无头像写入";
    delete me;
}

// ---------------------------------------------------------------------------
// 6) 通用缺省与体重
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, Defaults_GenderAndSpeak_UndefinedFilled)
{
    CChar* me = MakeNpc(30);
    setup_human(me);

    EXPECT_STREQ(me->querystr("gender"), "男性");
    EXPECT_EQ(me->query("can_speak"), 1);
    delete me;
}

TEST_F(SetupHumanTest, Defaults_GenderAndSpeak_DefinedKept)
{
    CChar* me = MakeNpc(30);
    me->set("gender", "女性");
    me->set("can_speak", (long)0);
    setup_human(me);

    EXPECT_STREQ(me->querystr("gender"), "女性");
    EXPECT_EQ(me->query("can_speak"), 0);
    delete me;
}

TEST_F(SetupHumanTest, Weight_Undefined_ComputedFromStr)
{
    CChar* me = MakeNpc(30);
    setup_human(me);

    // 40000 + (20-10)*2000 = 60000
    EXPECT_EQ(me->query_weight(), 60000);
    delete me;
}

TEST_F(SetupHumanTest, Weight_Defined_NotOverwritten)
{
    CChar* me = MakeNpc(30);
    me->set_weight(55);
    setup_human(me);

    EXPECT_EQ(me->query_weight(), 55);
    delete me;
}

// ---------------------------------------------------------------------------
// 7) 拆分后的子函数直调：各自可独立执行且不越界
// ---------------------------------------------------------------------------
TEST_F(SetupHumanTest, Direct_BaseAttrs_TouchesOnlyItsSixKeys)
{
    CChar* me = new CChar();
    setup_human_base_attrs(me);

    EXPECT_GE(me->query("age"), 15);
    EXPECT_LE(me->query("age"), 44);
    for (const char* key : {"str", "con", "int", "dex", "per"}) {
        EXPECT_GE(me->query(key), 10) << key;
        EXPECT_LE(me->query(key), 30) << key;
    }
    EXPECT_EQ(me->query("can_speak"), 0) << "不应顺带写 gender/can_speak";
    EXPECT_EQ(me->query("max_hp"), 0) << "不应顺带算 max_hp";
    delete me;
}

TEST_F(SetupHumanTest, Direct_MaxHp_RecomputesWithoutGate)
{
    // 子函数无门控：调用即重算（userp/undefinedp 判断属于编排层）
    CChar* me = MakeNpc(30);
    me->set("max_hp", (long)999);
    setup_human_max_hp(me, 30);

    EXPECT_EQ(me->query("max_hp"), 1220);
    EXPECT_EQ(me->query_temp("max_hp"), 1220) << "temp 同步仍在子函数内";
    delete me;
}

TEST_F(SetupHumanTest, Direct_NpcSetup_FillsMpUnitAttitude)
{
    CChar* me = MakeNpc(30);
    me->set_skill("force", 100);
    setup_human_npc(me);

    EXPECT_EQ(me->query("max_mp"), 1000);
    EXPECT_EQ(me->query("mp"), 2000);
    EXPECT_STREQ(me->querystr("unit"), "位");
    EXPECT_STREQ(me->querystr("attitude"), "peaceful");
    EXPECT_EQ(me->query("age"), 30) << "不应改动与本功能集无关的属性";
    delete me;
}

// ---------------- 内功气血加成钩子缺席 ----------------
TEST_F(SetupHumanTest, ForceBonus_HookAbsent_NoBonus)
{
    // 钩子缺席（dataset 未提供该能力）：只保留基础气血，不追加内功加成
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(luaL_dostring(L,
                            "__orig_fb = hooks.setup_human "
                            "hooks.setup_human = nil"),
              LUA_OK);

    CChar* me = MakeNpc(30);
    me->set_skill("taoism", 100);
    me->set_skill("taiji_shengong", 60);
    setup_human(me);

    luaL_dostring(L, "hooks.setup_human = __orig_fb __orig_fb = nil");
    EXPECT_EQ(me->query("max_hp"), 1220) << "缺席时无加成（仅基础 1220）";
    delete me;
}

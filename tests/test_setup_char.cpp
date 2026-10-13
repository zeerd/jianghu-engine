// test_setup_char.cpp — setup_char/human 的内功气血加成开关测试
//
// 契约：config.ini [server] force_add_hp 打开时，内功等级计入最大气血；
// 关闭时最大气血只由先天属性与年龄决定。两次输入完全相同，唯一差异是开关。
#include "pch.h"

#include "Char.h"
#include "IniParser.h"
#include "ServerConfig.h"

namespace {

class SetupCharTest : public ::testing::Test {
protected:
    void SetUp() override { ApplyConfig(false); }

    void TearDown() override { ApplyConfig(false); }

    static void ApplyConfig(bool force_add_hp)
    {
        ini_load("tests/test_config.ini");
        g_ini_data["server"]["force_add_hp"] = force_add_hp ? "1" : "0";
        CServerConfig::init();
    }

    // 属性固定、不携带任何会影响气血的角色，保证结果只随开关变化
    static CChar* MakeHuman()
    {
        CChar* me = new CChar();
        me->set("id", "setup1");
        me->set("name", "测试角色");
        me->set("age", (long)30);
        me->set("str", (long)20);
        me->set("con", (long)20);
        me->set("int", (long)20);
        me->set("dex", (long)20);
        me->set("per", (long)20);
        me->set("level", (long)10);
        me->set_skill("force", 100);
        return me;
    }

    static long MaxHpWith(bool force_add_hp)
    {
        ApplyConfig(force_add_hp);
        CChar* me = MakeHuman();
        setup_char(me);
        long max_hp = me->query("max_hp");
        delete me;
        return max_hp;
    }
};

}  // namespace

TEST_F(SetupCharTest, ForceAddHpOff_UsesBaseFormulaOnly)
{
    long off = MaxHpWith(false);

    // hp = con(20)，level=10，age=30 -> 300 + 10*20*3 + (30-14)*20
    EXPECT_EQ(off, 1220);
}

TEST_F(SetupCharTest, ForceAddHpOn_BoostsMaxHpByForceSkill)
{
    long off = MaxHpWith(false);
    long on  = MaxHpWith(true);

    EXPECT_GT(on, off) << "force_add_hp 打开时内功等级应计入最大气血";
}

// ---------------------------------------------------------------------------
// race 分派：野兽 / 妖魔 / 未知种族
// ---------------------------------------------------------------------------
TEST_F(SetupCharTest, Beast_RaceDispatch_FillsUnitAttrsAndAgeLadder)
{
    CChar* me = new CChar();
    me->set("id", "beast1");
    me->set("name", "测试野兽");
    me->set("race", "野兽");
    me->set("level", (long)10);
    setup_char(me);

    EXPECT_STREQ(me->querystr("unit"), "只");

    const long age = me->query("age");
    EXPECT_GE(age, 5);
    EXPECT_LE(age, 44);
    for (const char* key : {"str", "int", "per", "con", "dex"}) {
        EXPECT_GE(me->query(key), 5) << key;
        EXPECT_LE(me->query(key), 45) << key;
    }

    // 年龄三段阶梯：<=5 / <=20 / 其余（level=10）
    long expected;
    if (age <= 5)
        expected = 150;
    else if (age <= 20)
        expected = 150 + (age - 5) * 25 * 10;
    else
        expected = 950 + (age - 20) * 5 * 10;
    EXPECT_EQ(me->query("max_hp"), expected);
    EXPECT_EQ(me->query("hp"), expected) << "NPC 的 hp 补满至上限";
    EXPECT_EQ(me->query_weight(), 40000 + (me->query("str") - 10) * 2000);
    delete me;
}

TEST_F(SetupCharTest, Monster_RaceDispatch_SkipsCharacterDefaults)
{
    CChar* me = new CChar();
    me->set("id", "mon1");
    me->set("name", "测试妖魔");
    me->set("race", "妖魔");
    me->set("level", (long)10);
    me->set("max_hp", (long)300);
    setup_char(me);

    EXPECT_EQ(me->query("age"), 0) << "妖魔不套用人类/野兽的年龄缺省";
    EXPECT_EQ(me->query("str"), 0) << "妖魔不套用属性缺省";
    EXPECT_EQ(me->query("hp"), 300);
    delete me;
}

TEST_F(SetupCharTest, UnknownRace_StillFillsHpAndEffHp)
{
    CChar* me = new CChar();
    me->set("id", "odd1");
    me->set("name", "未知种族");
    me->set("race", "异族");
    me->set("level", (long)10);
    me->set("max_hp", (long)500);
    setup_char(me);

    EXPECT_STREQ(me->querystr("race"), "异族") << "未知种族不被改写";
    EXPECT_EQ(me->query("hp"), 500);
    EXPECT_EQ(me->query("eff_hp"), 500);
    delete me;
}

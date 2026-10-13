/**
 * @file SetupChar.cpp */
#include <windows.h>

#include "Char.h"
#include "LuaHook.h"
#include "ServerConfig.h"

// 随机先天属性缺省：六项属性只在未定义时写入
void setup_human_base_attrs(CChar* me)
{
    CMapping* m = me->query_entire_dbase();

    if (m->undefinedp("age")) m->set("age", g_utils.random(30) + 15);

    if (m->undefinedp("str")) m->set("str", g_utils.random(21) + 10);
    if (m->undefinedp("con")) m->set("con", g_utils.random(21) + 10);
    if (m->undefinedp("int")) m->set("int", g_utils.random(21) + 10);
    if (m->undefinedp("dex")) m->set("dex", g_utils.random(21) + 10);
    if (m->undefinedp("per")) m->set("per", g_utils.random(21) + 10);
}

// 最大气血：年龄阶梯公式、珍果、下限与 temp 同步
void setup_human_max_hp(CChar* me, int age)
{
    CMapping* m = me->query_entire_dbase();

    int hp = m->query("con");
    if (CServerConfig::force_add_hp()) hp += me->query_skill("force", 1) / 10;

    // 最大上限与等级的关系
    int base_value = 300 + m->query("level") * hp * 3;

    if (age <= 14)
        m->set("max_hp", base_value);
    else if (age <= 40)
        m->set("max_hp", base_value + (age - 14) * hp);
    else if (age <= 100)
        m->set("max_hp", base_value + age + int((age - 14) * hp * 5 / 6));
    else if (age <= 150)
        m->set("max_hp", base_value + age * 2 + int((age - 14) * hp * 4 / 5));
    else if (age <= 200)
        m->set("max_hp", base_value + age * 3 + int((age - 14) * hp * 3 / 4));
    else if (age <= 250)
        m->set("max_hp", base_value + age * 4 + int((age - 14) * hp * 2 / 3));
    else if (age <= 300)
        m->set("max_hp", base_value + age * 5 + int((age - 14) * hp / 2));
    else
        m->set("max_hp", base_value + age * 10 + (age - 14) * hp / 10);

    // 假如吃过后天珍果，则最大HP永久提高
    if (me->query("add_hp")) me->add("max_hp", me->query("add_hp"));

    if (m->query("max_hp") < 1) m->set("max_hp", 1);
    me->set_temp("max_hp", me->query("max_hp"));
}

// NPC 侧：MP 推导与 unit/attitude 缺省
void setup_human_npc(CChar* me)
{
    CMapping* m = me->query_entire_dbase();

    if (m->undefinedp("max_mp")) {
        int mp = me->query_skill("force") * 20;
        m->set("max_mp", mp);
    }

    m->set("mp", m->query("max_mp") * 2);

    if (m->undefinedp("unit")) m->set("unit", "位");
    if (m->undefinedp("attitude")) m->set("attitude", "peaceful");
}

// 人类初始化编排：属性缺省 → 最大气血 → 通用缺省 → NPC/玩家分支 → 体重
void setup_human(CChar* me)
{
    CMapping* m = me->query_entire_dbase();

    setup_human_base_attrs(me);

    int age = m->query("age");
    if (userp(me) || m->undefinedp("max_hp")) setup_human_max_hp(me, age);

    if (m->undefinedp("gender")) m->set("gender", "男性");
    if (m->undefinedp("can_speak")) m->set("can_speak", 1);

    if (userp(me)) {
        if (m->undefinedp("level")) m->set("level", 1);
    }
    else
        setup_human_npc(me);

    if (!me->query_weight())
        me->set_weight(40000 + (m->query("str") - 10) * 2000);

    // 初始化钩子（hooks.setup_human）
    CLuaHookCall hook("setup_human");
    hook.add_object(me);
    hook.call();
}

void setup_monster(CChar* me) {}

void setup_beast(CChar* me)
{
    CMapping* m = me->query_entire_dbase();

    m->set("unit", "只");

    if (m->undefinedp("gender")) m->set("gender", "雄性");
    if (m->undefinedp("age")) m->set("age", g_utils.random(40) + 5);

    if (m->undefinedp("str")) m->set("str", g_utils.random(41) + 5);
    if (m->undefinedp("int")) m->set("int", g_utils.random(11) + 5);
    if (m->undefinedp("per")) m->set("per", g_utils.random(31) + 5);
    if (m->undefinedp("con")) m->set("con", g_utils.random(41) + 5);
    if (m->undefinedp("dex")) m->set("dex", g_utils.random(41) + 5);

    if (m->undefinedp("max_hp")) {
        int level = m->query("level");
        int age   = m->query("age");

        if (age <= 5)
            m->set("max_hp", 150);
        else if (age <= 20)
            m->set("max_hp", 150 + (age - 5) * 25 * level);
        else
            m->set("max_hp", 950 + (age - 20) * 5 * level);
    }

    if (!me->query_weight())
        me->set_weight(40000 + (m->query("str") - 10) * 2000);
}

void setup_char(CChar* me)
{
    string race;
    CMapping* m = me->query_entire_dbase();

    race = m->querystr("race");

    if (!race.length()) {
        race = "人类";
        m->set("race", "人类");
    }

    // 设置NPC的等级
    if (!userp(me) && m->undefinedp("level"))
        m->set("level", CChar::exp2level(m->query("combat_exp")));

    if (race == "人类")
        setup_human(me);
    else if (race == "妖魔")
        setup_monster(me);
    else if (race == "野兽")
        setup_beast(me);
    else
        LOG_ERROR("Chard: undefined race %s。", race.c_str());

    if (!userp(me) || m->undefinedp("hp"))  // 将NPC的hp上限补满
        m->set("hp", m->query("max_hp"));

    if (m->undefinedp("eff_hp")
        || m->query("eff_hp") > m->query("max_hp"))  // 让有效上限不超过最大值。
        m->set("eff_hp", m->query("max_hp"));

    if (m->undefinedp("repute") && !userp(me)) {
        m->set("repute", m->query("shen_type") * m->query("level") * 100);
    }

    if (!me->query_max_encumbrance()) {
        me->set_max_encumbrance(m->query("str") * 5000
                                + me->query_unarmed() * 4000);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// setup_char(me) - 初始化角色属性
int l_setup_char(lua_State* L)
{
    CChar* me = dynamic_cast<CChar*>(get_object(L, 1));
    if (!me) return 0;
    setup_char(me);
    return 0;
}

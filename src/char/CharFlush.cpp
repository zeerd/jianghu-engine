/**
 * @file CharFlush.cpp
 * @brief implementation of CChar (CharFlush). */

#include <windows.h>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Skill.h"
#include "CmdHandle.h"
#include "Room.h"
#include "Condition.h"
#include "Channel.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "AutoFight.h"
#include "CombatConfig.h"
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"

void CChar::flush_my_info()
{
    // 获得角色信息
    char msg[512], str[255];
    static char szAttr[11][20]
        = {"age",    "level",  "repute", "combat_exp", "hp", "mp",
           "max_hp", "max_mp", "eff_hp", "pks",        "mks"};
    static char szA[11][2]
        = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "m", "n"};

    // 投递能力而非身份：没有客户端时 send_command 必然是空操作，
    // 整个函数体（含 3 次 skill_power）都不必执行。
    if (!has_client()) return;

    int32_t sk_exp    = _M.query("combat_exp") / 3 - _M.query("sk_exp");
    int32_t valid_exp = query_valid_exp();

    snprintf(msg, 255,
             "&C=%d&T=%d&o=%s&p=%s&P=%d&q=%s&r=%s&s=%s&t=%s&u=%d&v=%d&exp=%"
             "d&nex=%d&sk=%d&master=%s&",
             RTNINFO, object_id(), id(), name(), _M.query("icon"),
             _M.querystr("gender"), _M.querystr("class"), _M.querystr("race"),
             title(), query_encumb(), query_max_encumbrance(), valid_exp,
             CChar::level2_exp(_M.query("level") + 1)
                 - CChar::level2_exp(_M.query("level")),
             sk_exp < valid_exp ? sk_exp : valid_exp,
             _M.querystr("family/master_name"));

    // 输出属性
    for (int i = 0; i < 11; i++) {
        snprintf(str, 255, "%s=%d&", szA[i], _M.query(szAttr[i]));
        strcat(msg, str);
    }

    snprintf(str, 255, "E=%d&F=%d&G=%d&H=%d&", query_str(), query_con(),
             query_int(), query_dex());
    strcat(msg, str);

    // 计算攻防
    const char* skill_type;
    CContainer* weapon;
    unsigned long ap, dp, pp;

    CVector v;
    CMapping* prepare = query_skill_prepare();
    prepare->keys(v);

    // 1, 选用哪种动作：依赖武器的技能。
    if ((weapon = query_weapon()))
        skill_type = weapon->querystr("skill_type");
    else if (v.count() == 0)
        skill_type = "unarmed";
    else
        skill_type = v[0];

    ap = (unsigned long)CCombatCalc::skill_power(this, skill_type,
                                                 SKILL_USAGE_ATTACK);
    pp = (unsigned long)CCombatCalc::skill_power(this, skill_type,
                                                 SKILL_USAGE_DEFENSE);
    dp = (unsigned long)CCombatCalc::skill_power(this, "dodge",
                                                 SKILL_USAGE_DEFENSE);

    if (ap > 4200000000) ap = 4200000000;
    if (pp > 4200000000) pp = 4200000000;
    if (dp > 4200000000) dp = 4200000000;
    unsigned long att, def;
    att = ap / 100 + 1;
    def = dp / 100 + (weapon ? pp : (pp / 10)) / 100 + 1;
    snprintf(str, 255, "I=%lu&J=%d&K=%lu&L=%d&", att,
             query_temp("apply/damage"), def, query_temp("apply/armor"));

    strcat(msg, str);

    send_command(msg, SEND_WAIT_BUFFERED);
}

void CChar::flush_my_equip()
{
    DTItemList* list;
    CContainer* t;
    int i = 0;
    char str[40], msg[512];

    // 投递能力而非身份：同 flush_my_info，无客户端则无产出。
    if (!has_client()) return;

    snprintf(msg, 40, "&C=%d&B=0&T=%d&", RTNITEM, object_id());

    list = &m_item_list;
    for (auto val : *list) {
        if (i && i % 10 == 0) {
            send_command(msg, SEND_WAIT_BUFFERED);
            snprintf(msg, 40, "&C=%d&B=%d&T=%d&", RTNITEM, i, object_id());
        }

        t = val;
        snprintf(str, 40, "I%d=%d&", i, t->object_id());
        strcat(msg, str);

        if (t->query("equipped"))  // 装备了要发个装备标志
        {
            snprintf(str, 40, "E%d=%d&", i, t->query("equipped"));
            strcat(msg, str);
        }
        i++;
    }

    send_command(msg, SEND_WAIT_BUFFERED);
}

void CChar::flush_my_skill(CChar* me)
{
    const char* map_skill;
    char str[40], msg[512];

    // 发送技能指令: C=RTNSKILL T=目标 B=1 E=1 开始结束标志 S=技能 L=级别
    // M=Map技能 P=me升级所需之经验点
    int pt = me->learn_bonus();

    CVector v;

    skills.keys(v);
    int cnt = v.count();
    for (int i = 0; i < cnt; i++) {
        const char* skl     = v[i];
        int32_t need_points = me->skill_need_points(skl);

        snprintf(msg, 512, "&C=%d&T=%d&S=%s&L=%d&", RTNSKILL, object_id(), skl,
                 skills[skl]);

        if (this == me)  // 当who == me时才传升级所需之点数
        {
            snprintf(str, 40, "P=%d&", need_points / pt + 1);
            strcat(msg, str);
        }

        map_skill = query_skill_mapped(skl);
        if (map_skill[0]) {
            snprintf(str, 40, "M=%s&", map_skill);
            strcat(msg, str);
        }

        // 传装备标记
        map_skill = query_skill_prepared(skl);
        if (map_skill[0]) {
            strcat(msg, "K=1&");
        }

        if (i == 0) strcat(msg, "B=1&");

        if (i == cnt - 1) strcat(msg, "E=1&");

        me->send_command(msg, SEND_WAIT_BUFFERED);
    }
}

void CChar::send_table(const char* table_name, CMapping* table, int total,
                       int page)
{
    char msg[512], tmp[255];
    CVector v;

    table->keys(v);
    int count = v.count();

    for (int i = 0; i < count; i++) {
        CMapping* line = table->querymap(v[i]);
        if (!line) continue;

        snprintf(msg, 255, "&C=%d&", SENDTABLE);
        if (i == 0) {
            if (i == count - 1)
                snprintf(tmp, 255, "st=3&T=%s&R=%d&P=%d&", table_name, total,
                         page);
            else
                snprintf(tmp, 255, "st=1&T=%s&R=%d&P=%d&", table_name, total,
                         page);
            strcat(msg, tmp);
        }
        else if (i == count - 1)
            strcat(msg, "st=2&");

        CVector v2;
        line->keys(v2);

        for (int j = 0; j < v2.count(); j++) {
            snprintf(tmp, 255, "%s=%s&", v2[j], line->querystr(v2[j]));
            strcat(msg, tmp);
        }

        send_command(msg);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:FlushMyInfo() -> void
int l_flush_my_info(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->flush_my_info();
    return 0;
}

// self:FlushMySkill() - 刷新玩家技能显示（CChar）
int l_flush_my_skill(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->flush_my_skill(ch);
    return 0;
}

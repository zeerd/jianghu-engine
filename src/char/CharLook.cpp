/**
 * @file CharLook.cpp
 * @brief implementation of CChar (CharLook). */

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
#include "LuaHook.h"

const char* CChar::inventory_look(CContainer* obj, int flag)
{
    static char str[80];

    str[0] = 0;
    if (obj->query("equipped"))
        snprintf(str, 80, "%s", obj->name());
    else if (flag) {
        int32_t amount = obj->query_amount();
        if (amount > 1)
            snprintf(str, 80, "数量%d的%s", amount, obj->name());
        else
            snprintf(str, 80, "%s", obj->name());
    }

    return str;
}

int CChar::do_look(CChar* me)
{
    char msg[255], str[40];
    char ability_evaluate[15][20] = {
        "不堪一击", "粗通皮毛", "半生不熟", "已有小成", "驾轻就熟",
        "出类拔萃", "神乎其技", "出神入化", "登峰造极", "一代宗师",
        "深不可测", "深不可测", "深不可测", "深不可测", "前无古人，后无来者",
    };

    int per = query_per();
    if (!per) per = g_utils.random(20);

    char pro[5];

    strcpy(pro, "你");
    if (me != this) {
        tell_object(this,
                    snprintf(msg, 255, "%s正盯著你看，不知道打些什么主意。",
                             me->name()));
        strcpy(pro, gender_pronoun(querystr("gender")));
    }

    say(snprintf(msg, 255, "%s %s", short_desc(), long_desc()), me);

    char resident[40];

    resident[0] = 0;
    if (EQUALSTR(querystr("race"), "人类") && query("age")) {
        if (querystr("resident")[0])
            snprintf(resident, 40, "%s看起来好象是%s人士。", pro,
                     querystr("resident"));
    }

    CMapping* skl = query_skills();
    int skills    = 0;

    if (skl->get_count()) {
        CVector v;

        skl->keys(v);
        for (int i = 0; i < v.count(); i++) skills += (*skl)[v[i]];

        skills = skills / (10 * skl->get_count());
        skills += 3 - g_utils.random(6);
    }

    if (skills < 0) skills = 0;
    if (skills >= 15) skills = 14;

    char family_des[80];

    family_des[0] = 0;
    int gen       = query("family/generation");
    int mygen     = me->query("family/generation");

    if (this != me && me->querymap("family")
        && EQUALSTR(querystr("family/family_name"),
                    me->querystr("family/family_name"))) {
        if (gen == mygen) {
            if (EQUALSTR(querystr("gender"), "男性")
                || EQUALSTR(querystr("gender"), "无性")) {
                snprintf(family_des, 80, "是你的%s%s。",
                         EQUALSTR(querystr("family/master_id"),
                                  me->querystr("family/master_id"))
                             ? ""
                             : "同门",
                         query("enter_time") > me->query("enter_time")
                             ? "师兄"
                             : "师弟");
            }
            else {
                snprintf(family_des, 80, "是你的%s%s。",
                         EQUALSTR(querystr("family/master_id"),
                                  me->querystr("family/master_id"))
                             ? ""
                             : "同门",
                         query("enter_time") > me->query("enter_time")
                             ? "师姐"
                             : "师妹");
            }
        }
        else if (gen < mygen) {
            if (EQUALSTR(me->querystr("family/master_id"), querystr("id")))
                snprintf(family_des, 80, "是你的师父。");
            else if (gen - mygen > 1)
                strcpy(family_des, "是你的同门长辈。");
            else if (query("family/enter_time")
                     < me->query("family/enter_time"))
                strcpy(family_des, "是你的师伯。");
            else
                strcpy(family_des, "是你的师叔。");
        }
        else {
            if (gen - mygen > 1)
                strcpy(family_des, "是你的同门晚辈。");
            else if (EQUALSTR(querystr("family/master_id"), me->querystr("id")))
                strcpy(family_des, "是你的弟子。");
            else
                strcpy(family_des, "是你的师侄。");
        }
    }

    char couple_des[80];

    couple_des[0] = 0;
    if (this != me && querymap("couple")
        && EQUALSTR(querystr("couple/id"), me->id(1))) {
        snprintf(couple_des, 80, "%s是你的%s。", pro,
                 me->querystr("couple/gender"));
    }

    char eff_msg[80];

    eff_msg[0] = 0;
    if (query("max_hp")) {
        snprintf(eff_msg, 80, "%s%s", pro,
                 eff_status_msg(query("eff_hp") * 100 / query("max_hp")));
    }

    if (DIFFERSTR(querystr("race"), "野兽")) {
        say(snprintf(msg, 255,
                     "%s看起来约%s多岁。%s%s%s%s武功看起来%s。%s%s%s%s", pro,
                     chinese_number(query("age") / 10 * 10, str), resident,
                     wizardp(this) ? "是巫师。" : "",
                     (query("age") > 15 || wizardp(me))
                         ? query_waimao(querystr("gender"), per)
                         : "",
                     pro, ability_evaluate[skills], family_des[0] ? pro : "",
                     family_des, couple_des, eff_msg),
            me);
    }

    // 身上带的物品
    DTItemList* list = get_item_list();

    if (list->size()) {
        CContainer* item;
        int flag = query_attr(IS_CORPSE);
        if (me->query("grant") >= 70 && me != this) flag = 1;

        for (auto val : *list) {
            item = val;
            add_menu_item(
                inventory_look(item, flag),
                flag ? snprintf(msg, 80, "$0get %ld from $1", item->object_id())
                     : "",
                me);
        }

        add_menu_item("结束观察", "", me);
    }

    int32_t me_shen  = me->query("repute");
    int32_t obj_shen = query("repute");

    if (this != me && living(this)
        && ((me_shen < 0 && obj_shen > 0) || (me_shen > 0 && obj_shen < 0))
        && ((me_shen - obj_shen > query("mp") * 20)
            || (obj_shen - me_shen > query("mp") * 20))) {
        tell_object(me, snprintf(msg, 255, "%s突然转过头来瞪你一眼。", name()));

        if (query("age") > 15 && me->query("age") > 15
            && (me->query("per") < query("per") || me->query("per") < 20)) {
            // 增加观察引发战斗者为主动PK者的设置
            me->set_temp("pending/PK", g_current_time);
            g_auto_fight.auto_fight(this, me, "berserk");
        }
    }

    // 观察扩展内容钩子（hooks.do_look）
    CLuaHookCall hook("do_look");
    hook.add_object(this).add_object(me);
    hook.call();

    return 0;
}

const char* CChar::eff_status_msg(int ratio)
{
    if (ratio == 100) return "看起来气血充盈，并没有受伤。";
    if (ratio > 95) return "似乎受了点轻伤，不过光从外表看不大出来。";
    if (ratio > 90) return "看起来可能受了点轻伤。";
    if (ratio > 80) return "受了几处伤，不过似乎并不碍事。";
    if (ratio > 60) return "受伤不轻，看起来状况并不太好。";
    if (ratio > 40) return "气息粗重，动作开始散乱，看来所受的伤著实不轻。";
    if (ratio > 30) return "已经伤痕累累，正在勉力支撑著不倒下去。";
    if (ratio > 20) return "受了相当重的伤，只怕会有生命危险。";
    if (ratio > 10) return "伤重之下已经难以支撑，眼看就要倒在地上。";
    if (ratio > 5) return "受伤过重，已经奄奄一息，命在旦夕了。";
    return "受伤过重，已经有如风中残烛，随时都可能断气。";
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// eff_status_msg(ratio) -> string
// （CCombatd::eff_status_msg：按气血比例返回状态描述）
int l_eff_status_msg(lua_State* L)
{
    int ratio     = (int)luaL_optinteger(L, 1, 100);
    const char* s = CChar::eff_status_msg(ratio);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// do_emote(me, target, verb, channel_emote, who)
int l_do_emote(lua_State* L)
{
    CChar* me     = NULL;
    CChar* target = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CChar*>(get_object(L, 1));
    if (lua_isuserdata(L, 2)) target = dynamic_cast<CChar*>(get_object(L, 2));
    const char* verb  = luaL_optstring(L, 3, "");
    int channel_emote = (int)luaL_optinteger(L, 4, 0);
    std::string who_gbk;
    if (lua_isstring(L, 5)) who_gbk = lua_tostring(L, 5);
    const char* who = who_gbk.empty() ? NULL : who_gbk.c_str();
    char* rtn       = g_channel.do_emote(me, target, verb, channel_emote, who);
    if (rtn) {
        lua_pushstring(L, rtn);
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

/**
 * @file CReward.cpp
 * @brief 战斗奖励函数 */

#include <windows.h>
#include <cstring>

#include "CReward.h"
#include "Channel.h"
#include "User.h"
#include "Container.h"
#include "Global.h"
#include "LuaHook.h"
#include "LuaQuest.h"
#include "PartyConfig.h"
#include "CombatRules.h"

extern int g_current_time;

CReward::CReward() {}

// 计算受害者损失的经验
int CReward::victim_lose(CChar* killer, CChar* victim)
{
    int32_t lose, exp_k, exp_v;

    if ((wizardp(victim) && !wizardp(killer))
        || (!wizardp(victim) && wizardp(killer)))
        return 0;

    exp_k              = killer->query("combat_exp");
    exp_v              = victim->query("combat_exp");
    int32_t valid_lose = exp_v - victim->query("sk_exp");

    if (exp_k > exp_v)
        lose = 10 * exp_v / (exp_k + 1) * (4000 + exp_v) / 1500;
    else
        lose = (4000 + exp_v) / 150;

    if (lose > valid_lose) lose = valid_lose;
    if (lose < 0) lose = 0;
    return lose;
}

// 受害者死亡惩罚
void CReward::victim_penalty(CChar* victim)
{
    int combat_exp_loss
        = (victim->query("combat_exp") - victim->query("sk_exp")) / 40;
    combat_exp_loss /= 4;

    if (combat_exp_loss < 0) combat_exp_loss = 0;

    victim->set("death/combat_exp_loss", combat_exp_loss);
    victim->add("combat_exp", -combat_exp_loss);
    victim->reset_level();
    victim->add("mds", 1);

    LOG_INFO("%s死了，损失%ld点经验。", victim->name(), combat_exp_loss);
    victim->set("death/skill_loss", 1);
    victim->skill_death_penalty();
}

// 计算NK基础经验值
int CReward::calc_nkgain_base(CChar* victim)
{
    int32_t eff_dx = victim->query("combat_exp");
    int32_t nkgain = victim->query("nkgain");

    if (!nkgain) {
        if (eff_dx > 667000)
            nkgain = 300;
        else if (eff_dx > 333000)
            nkgain = 255;
        else if (eff_dx > 100000)
            nkgain = 195;
        else if (eff_dx > 33000)
            nkgain = 150;
        else if (eff_dx > 17000)
            nkgain = 99;
        else if (eff_dx > 5000)
            nkgain = 75;
        else if (eff_dx > 2000)
            nkgain = 50;
        else
            nkgain = 35;
    }
    return nkgain;
}

// 计算组队经验分配
int CReward::calc_party_exp(CChar* killer, int32_t nkgain, int32_t cx,
                            CUser*& player1, CUser*& player2, CUser*& player3)
{
    int32_t eff_dx = nkgain;
    int32_t dx     = cx * 2 / 3 * 2 / 3;
    double times   = (double)(eff_dx - dx / 2) / (double)(dx + 1);
    if (times > 2) times = 2;
    if (times < 0) times = 0;
    return (int32_t)(nkgain * times);
}

// 分配组队经验给队员
void CReward::distribute_party_exp(CChar* killer, CChar* victim, int32_t reward,
                                   int32_t cx, CUser* player1, CUser* player2,
                                   CUser* player3)
{
    char msg[255];
    int32_t exp, nk_limit;
    double bili;
    int count = killer->query_temp("party/battle_count");

    // 计算队长经验
    bili     = (double)(killer->query("combat_exp") + 1) / (double)(cx + 1);
    exp      = (int32_t)(reward * bili);
    nk_limit = killer->query("level") * 4 + 100;
    if (exp > nk_limit) exp = nk_limit;
    killer->add("combat_exp", exp);
    killer->add("kill/nkgain", exp);
    tell_object(killer, snprintf(msg, 255, "你杀死%s获得%d点NK经验！",
                                 victim->name(1), (int)exp));

    // 分配给队员1
    if (player1 && count >= 1) {
        bili = (double)(player1->query("combat_exp") + 1) / (double)(cx + 1);
        exp  = (int32_t)(reward * bili);
        nk_limit = player1->query("level") * 4 + 100;
        if (exp > nk_limit) exp = nk_limit;
        player1->add("combat_exp", exp);
        player1->add("kill/nkgain", exp);
        tell_object(player1, snprintf(msg, 255, "你杀死%s获得%d点NK经验！",
                                      victim->name(1), (int)exp));
    }

    // 分配给队员2
    if (player2 && count >= 2) {
        bili = (double)(player2->query("combat_exp") + 1) / (double)(cx + 1);
        exp  = (int32_t)(reward * bili);
        nk_limit = player2->query("level") * 4 + 100;
        if (exp > nk_limit) exp = nk_limit;
        player2->add("combat_exp", exp);
        player2->add("kill/nkgain", exp);
        tell_object(player2, snprintf(msg, 255, "你杀死%s获得%d点NK经验！",
                                      victim->name(1), (int)exp));
    }

    // 分配给队员3
    if (player3 && count >= 3) {
        bili = (double)(player3->query("combat_exp") + 1) / (double)(cx + 1);
        exp  = (int32_t)(reward * bili);
        nk_limit = player3->query("level") * 4 + 100;
        if (exp > nk_limit) exp = nk_limit;
        player3->add("combat_exp", exp);
        player3->add("kill/nkgain", exp);
        tell_object(player3, snprintf(msg, 255, "你杀死%s获得%d点NK经验！",
                                      victim->name(1), (int)exp));
    }

    killer->delete_temp("party/battle_count");
}

// NPC击杀经验奖励
int CReward::nk_gain(CChar* killer, CChar* victim)
{
    int32_t reward, nkgain;
    char msg[255];
    CUser* player1 = nullptr;
    CUser* player2 = nullptr;
    CUser* player3 = nullptr;

    // 如果配置关闭组队经验，且击杀者处于组队中，直接返回 0
    if (!CPartyConfig::share_exp()) {
        if (killer->querystr_temp("party/master")[0]
            || killer->query_temp("party/number")) {
            // 夫妻组队不受 share_exp 限制
            if (!killer->query_temp("party/couple")) {
                return 0;
            }
        }
    }

    // 防止晕倒的NPC给其他人得经验
    if (DIFFERSTR(victim->querystr_temp("last_fainted_from"),
                  killer->querystr("id"))
        && !killer->querymap_temp("party")
        && DIFFERSTR(killer->querystr("couple/id"),
                     victim->querystr_temp("last_fainted_from"))) {
        LOG_WARN("%s was killed by [%s], but fainted by [%s].", victim->name(1),
                 killer->querystr("id"),
                 victim->querystr_temp("last_fainted_from"));
        return 0;
    }

    if (DIFFERSTR(victim->querystr_temp("last_fainted_from"),
                  killer->querystr("id"))
        && killer->querymap_temp("party")) {
        if (killer->query_temp("party/number")) {
            CUser* pary_client
                = find_online(victim->querystr_temp("last_fainted_from"));
            if (!pary_client
                || !killer->query_temp(
                    snprintf(msg, 255, "party/friend_%s",
                             victim->querystr_temp("last_fainted_from"))))
                return 0;
            if (pary_client->query_temp("fight/fight_team")
                    != killer->query_temp("fight/fight_team")
                || !pary_client->query_temp("fight/is_fighting")
                || !killer->query_temp("fight/is_fighting")
                || DIFFERSTR((pary_client->environment())->querystr("name"),
                             (killer->environment())->querystr("name")))
                return 0;
        }
        else if (killer->querystr_temp("party/master")[0]) {
            CUser* pary_master
                = find_online(victim->querystr_temp("last_fainted_from"));
            if (!pary_master
                || DIFFERSTR(killer->querystr_temp("party/master"),
                             victim->querystr_temp("last_fainted_from")))
                return 0;
            if (pary_master->query_temp("fight/fight_team")
                    != killer->query_temp("fight/fight_team")
                || !pary_master->query_temp("fight/is_fighting")
                || !killer->query_temp("fight/is_fighting")
                || DIFFERSTR((pary_master->environment())->querystr("name"),
                             (killer->environment())->querystr("name")))
                return 0;
        }
    }

    // 得NPC的声望
    if (killer->query("level") < victim->query("level"))
        killer->add("repute", -victim->query("repute") / 10);

    // 不能杀同门派的NPC得经验
    if (killer->querymap("family")
        && EQUALSTR(killer->querystr("family/family_name"),
                    victim->querystr("family/family_name")))
        return 0;

    nkgain     = calc_nkgain_base(victim);
    int32_t cx = killer->query("combat_exp");

    // 假如玩家处于组队状态且启用经验共享
    if (CPartyConfig::share_exp() && killer->query_temp("party/number")) {
        CContainer* env1 = killer->environment();
        DTItemList* list = env1->get_item_list();
        for (auto val : *list) {
            CContainer* obj1 = val;
            if (!obj1->is_character()) continue;
            CChar* target = static_cast<CChar*>(obj1);
            if (!userp(target)) continue;
            if (target->query_temp("fight/fight_team")
                    != killer->query_temp("fight/fight_team")
                || !target->query_temp("fight/is_fighting"))
                continue;
            if (killer->query_temp(snprintf(msg, 255, "party/friend_%s",
                                            target->querystr("id")))) {
                int32_t tx = target->query("combat_exp");
                if (tx > cx) cx = tx;
                int32_t n = killer->add_temp("party/battle_count", 1);
                if (n == 1)
                    player1 = static_cast<CUser*>(target);
                else if (n == 2)
                    player2 = static_cast<CUser*>(target);
                else if (n == 3)
                    player3 = static_cast<CUser*>(target);
            }
        }
    }

    reward = calc_party_exp(killer, nkgain, cx, player1, player2, player3);

    if (killer->query_temp("party/battle_count") > 0) {
        distribute_party_exp(killer, victim, reward, cx, player1, player2,
                             player3);
        return 0;
    }
    return reward;
}

// PK惩罚
void CReward::pk_reward(CChar* killer, CChar* victim)
{
    char msg[255];
    int32_t ki, vi;
    time_t t;
    time(&t);

    if (static_cast<int32_t>(t) > killer->query("kill/free"))
        killer->del("kill/guilt");
    killer->set("kill/free", static_cast<int32_t>(t + 86400));

    ki = killer->query("kill/guilt");
    vi = killer->query("kill/guilt");

    const char* city = killer->querystr_temp("pending/PK_city");
    if (vi <= ki) {
        if (city[0])
            killer->add("kill/guilt", 50);
        else
            killer->add("kill/guilt", 10);
    }
    else {
        if (city[0])
            killer->add("kill/guilt", 10);
        else
            killer->add("kill/guilt", 5);
    }

    victim->add("kill/guilt", -20);
    if (victim->query("kill/guilt") < 0) victim->del("kill/guilt");

    killer->add("repute", (vi - ki) * 10);
    tell_object(killer, snprintf(msg, 255,
                                 "$HIY你PK玩家%s，获得了%d点声望。当前罪行%d。",
                                 victim->name(1), (int)((vi - ki) * 10),
                                 (int)killer->query("kill/guilt")));

    g_channel.do_channel(
        &g_channel, NULL, "sys",
        snprintf(msg, 255, "%s在%s行凶杀死%s。罪行：%ld。", killer->name(1),
                 city[0] ? city : "野外", victim->name(1),
                 killer->query("kill/guilt")));

    // PK 结算落地（罪行档位、悬捕条件等数据集规则）：guilt/repute 已更新、
    // 播报已发；返回值不消费，nil = 数据集不处理。
    combat_rule_yes("pk_reward", killer, victim);
}

// 处理PK奖励逻辑
void CReward::handle_pk_reward(CChar* killer, CChar* victim, int pkreward,
                               int pk_chang, int32_t lose, int32_t gain,
                               const char* killerid, CContainer* env)
{
    char msg[255];
    victim->delete_temp("last_fainted_from");

    if (killerid == victim->querystr("kill/last_killer"))
        gain = victim->query("kill/last_lose") / 2;
    gain = 0;
    if (gain < 0) gain = 0;

    killer->add("kill/pkgain", gain);
    killer->add("combat_exp", gain);
    victim->set("kill/kill_time", g_current_time);
    victim->set("kill/last_killer", killerid);
    victim->set("kill/last_lose", gain);
    victim->add("kill/pklose", lose);
    victim->add("combat_exp", -lose);
    victim->reset_level();
    victim->set("death/combat_exp_loss", lose);
    victim->set("death/skill_loss", 1);
    victim->skill_death_penalty();
    victim->add("pds", 1);

    snprintf(msg, 255, "%s(%s)被%s(%s)杀了，损失%d点经验。", victim->name(1),
             victim->id(1), killer->name(1), killer->id(1), (int)lose);
    g_channel.do_channel(&g_channel, NULL, "sys", msg);
    LOG_INFO("%s", msg);

    int32_t ki, vi;
    int pk = 1;
    if (!(ki = killer->query_temp("pending/PK")))
        pk = 0;
    else {
        killer->delete_temp("pending/PK");
        if ((vi = victim->query_temp("pending/PK"))) {
            victim->delete_temp("pending/PK");
            if (vi < ki) pk = 0;
        }
    }
    if (!pkreward && pk) pk_reward(killer, victim);

    char ti1[20], ti2[20];
    if (killer->querymap("family"))
        strncpy(ti1, killer->querystr("family/family_name"), 20);
    else
        strncpy(ti1, "百姓", 20);
    if (victim->querymap("family"))
        strncpy(ti2, victim->querystr("family/family_name"), 20);
    else
        strncpy(ti2, "百姓", 20);

    if (pkreward)
        snprintf(msg, 255, "%s%s在%s杀死仇敌%s%s，得经验%d点。", ti1,
                 killer->name(), env->name(), ti2, victim->name(), (int)gain);
    else
        snprintf(msg, 255, "%s%s在%s杀死%s%s，得经验%d点。", ti1,
                 killer->name(), env->name(), ti2, victim->name(), (int)gain);

    if (!pk_chang)
        g_channel.do_channel(&g_channel, 0, "rumor", msg);
    else
        tell_room(env, msg);
}

// 处理NPC击杀奖励
void CReward::handle_npc_kill_reward(CChar* killer, CChar* victim,
                                     int32_t reward)
{
    if (reward <= 0) return;

    char msg[255];
    int nk_limit = killer->query("level") * 4 + 100;
    if (reward > nk_limit) reward = nk_limit;

    // 击杀奖励的数据集结算（彩票、经验倍率、朱果配额）：脚本在场则以其
    // 返回的 reward 入账，缺席/未返回则按 C++ 封顶值入账。
    CLuaHookCall hook("kill_bonus");
    if (hook.valid()) {
        hook.add_object(killer).add_object(victim).add_int(reward);
        if (hook.call() && !hook.result_is_nil()) {
            int64_t bonus = reward;
            if (hook.table_field_int("reward", bonus))
                reward = static_cast<int32_t>(bonus);
        }
    }

    killer->add("kill/nkgain", reward);
    killer->add("combat_exp", reward);

    tell_object(killer, snprintf(msg, 255, "你杀死%s获得%d点NK经验！",
                                 victim->name(1), (int)reward));
}

// 杀手奖励主函数
void CReward::killer_reward(CChar* killer, CChar* victim)
{
    int32_t lose, gain;
    char msg[255];

    if (victim->query_attr(CChar::IS_USER)) {
        if (killer->query_attr(CChar::IS_USER)) {
            int pk_chang    = 0;
            CContainer* env = killer;
            while (env->environment()) env = env->environment();
            if (env->query("PK")) pk_chang = 1;

            int pkreward = 0;

            // 击杀结算：任一数据集规则命中（如仇敌名单递减）则奖励加倍
            if (combat_rule_yes("kill_settle", killer, victim)) pkreward++;

            const char* killerid = killer->querystr("id");
            lose                 = victim_lose(killer, victim);
            if (pkreward) lose = lose * 2;
            gain = lose * 8 / 10;
            if (gain < 0) gain = 0;

            handle_pk_reward(killer, victim, pkreward, pk_chang, lose, gain,
                             killerid, env);
        }
        else {
            victim_penalty(victim);
            g_channel.do_channel(&g_channel, 0, "rumor",
                                 snprintf(msg, 255, "%s被%s杀死了。",
                                          victim->name(), killer->name()));
        }
    }
    else {  // NPC is the victim
        if (killer->query_attr(CChar::IS_USER)) {
            int reward;
            if (killer->querystr_temp("party/master")[0]) {
                CUser* pary_master
                    = find_online(killer->querystr_temp("party/master"));
                if (pary_master
                    && pary_master->query_temp("fight/fight_team")
                           == killer->query_temp("fight/fight_team")
                    && pary_master->query_temp("fight/is_fighting")
                    && killer->query_temp("fight/is_fighting")
                    && EQUALSTR((pary_master->environment())->querystr("name"),
                                (killer->environment())->querystr("name"))) {
                    reward = nk_gain(pary_master, victim);
                }
                else {
                    reward = nk_gain(killer, victim);
                }
            }
            else {
                reward = nk_gain(killer, victim);
            }

            handle_npc_kill_reward(killer, victim, reward);
            quest_npc_killed(killer, victim);
        }
        killer->add("mks", 1);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// killer_reward(killer, victim) - 击杀奖励（对应 g_Reward.Killer_Reward）
// 野外怪等 Lua 脚本 die() 中给凶手发经验/声望用（原 CNmonster::reward_exp）
int l_killer_reward(lua_State* L)
{
    CChar* killer = lua_check_char(L, 1);
    CChar* victim = lua_check_char(L, 2);
    if (killer && victim) g_reward.killer_reward(killer, victim);
    return 0;
}

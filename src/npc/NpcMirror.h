/**
 * @file NpcMirror.h
 * @brief 任务用标准 NPC 模板，可生成玩家倍率的怪物。
 */
#pragma once

#include "Skill.h"
#include "Npc.h"
#include "Utils.h"

/**
 * @brief 任务用 NPC 镜像基类，支持倍率属性与自动战斗 AI。
 */
NPC_BEGIN(CNpcMirror);

void die()
{
    CChar* me = (CChar*)find_object(query_temp("last_damage_from"), 1);

    if (me) reward_killer(me);

    message_vision("$N惨叫一声，化为灰烬。", this);
    DESTRUCT_OB(this, "CNpcMirror::die");
    return;
}

virtual void reward_killer(CChar* me) {}

void eat_yao(char* yaopin)
{
    char msg[40];
    CContainer* yao = g_item_mgr->load_item(yaopin);
    yao->move(this);
    command(snprintf(msg, 40, "use %ld", yao->object_id()));
}

// 释放绝招
char* chat_msg_combat()
{
    if (query("hp") < query("max_hp") / 3) {
        if (g_utils.random(2) == 0) {
            if (perform_action("force heal", 0)) return 0;
        }

        if (g_utils.random(3) == 0) {
            eat_yao("yaopin_1");
            return 0;
        }

        if (g_utils.random(3) == 0) {
            eat_yao("yaopin_2");
            return 0;
        }
    }

    if (g_utils.random(5) == 0 && !query("flag"))
        perform_action("force lieyan", 1);

    return 0;
}

// 制造玩家镜象 diff 为强度 1 ~ 20 要除10的。
void make_mirror(CChar* me, int diff)
{
    int lvl = 10 + me->query("level") * 3;
    if (lvl > 400) lvl = 400;  // 没必要让这个NPC成为高手的克形

    // 先设置一些基本技能
    set_skill("taishan_sword", lvl);
    set_skill("dodge", lvl);
    set_skill("xiaoyaoyou", lvl);
    set_skill("parry", lvl);
    set_skill("force", lvl);
    set_skill("magic", lvl);
    set_skill("unarmed", lvl);

    map_skill("sword", "taishan_sword");

    // 复制玩家状态
    CMapping *skill_status, *map_status, *prepare_status;

    skill_status = me->query_skills();

    CVector v;
    int i;

    skill_status->keys(v);

    for (i = 0; i < v.count(); i++) {
        set_skill(v[i], (*skill_status)[v[i]] * diff / 10);
    }

    map_status = me->query_skill_map();

    v.clear();

    map_status->keys(v);

    for (i = 0; i < v.count(); i++) {
        map_skill(v[i], map_status->querystr(v[i]));
    }

    prepare_status = me->query_skill_prepare();

    v.clear();

    prepare_status->keys(v);

    for (i = 0; i < v.count(); i++) {
        prepare_skill(v[i], prepare_status->querystr(v[i]));
    }

    set("str", me->query("str"));
    set("int", me->query("int"));
    set("con", me->query("con"));
    set("dex", me->query("dex"));

    set("max_hp", me->query("max_hp") * diff / 10);
    set("eff_hp", query("max_hp"));
    set("hp", query("max_hp"));
    set("max_mp", me->query("max_mp") * diff / 10);
    set("mp", query("max_mp") * 2);

    set("mp_factor", me->query("mp_factor") * diff / 10);
    set("combat_exp", me->query_exp() * diff / 10);
    set("level", me->query("level") * diff / 10);

    // 附加属性
    set_temp("apply/str", me->query_temp("apply/str") * diff / 10);
    set_temp("apply/dex", me->query_temp("apply/dex") * diff / 10);
    set_temp("apply/attack", me->query_temp("apply/attack") * diff / 10);
    set_temp("apply/defense", me->query_temp("apply/defense") * diff / 10);
    set_temp("apply/armor", me->query_temp("apply/armor") * diff / 10);
    set_temp("apply/damage", me->query_temp("apply/damage") * diff / 10);
    set_temp("apply/dodge", me->query_temp("apply/dodge") * diff / 10);
    set_temp("apply/parry", me->query_temp("apply/parry") * diff / 10);

    // 设置怪物名姓
    set_name(me->querystr_temp("fool/name"));

    // 设置怪物的法术
    map_skill("force", "magic");

    // 检查怪物的轻功等级：
    int limit = lvl + lvl / 2;
    if (query_skill("dodge") < limit)  // 如果玩家的技能实在太烂，只好用自己的。
        map_skill("dodge", "xiaoyaoyou");

    // 检查怪物的招架等级：
    if (query_skill("parry") < limit) map_skill("parry", "taishan_sword");

    CContainer* weapon = make_weapon(this);
    if (weapon) {
        weapon->move(this);
        if (auto* eq = dynamic_cast<IEquippable*>(weapon)) eq->wield();
    }
}

// 为me寻找最合手的武器
static CContainer* make_weapon(CChar* me)
{
    int level = 0, max_level = 0;

    CMapping* skill_status;
    CSkill* sk;
    string base_skill = "unarmed";

    skill_status = me->query_skills();

    CVector v;

    skill_status->keys(v);

    for (int i = 0; i < v.count(); i++) {
        const char* skl = v[i];
        sk              = load_skill(skl);
        if (sk->querystr("base_skill")[0]) continue;         // 高级技能
        if (strcmp(sk->type(), "knowledge") == 0) continue;  // 知识
        if (strcmp(skl, "force") == 0) continue;             // 内功除外
        if (strcmp(skl, "parry") == 0) continue;             // 招架除外
        if (strcmp(skl, "dodge") == 0) continue;             // 躲闪除外

        level = me->query_skill(skl);  // 基本技能
        if (max_level < level) {
            max_level  = level;
            base_skill = skl;
        }
    }

    if (!max_level) return 0;

    int lvl = 10 + me->query("level") * 3;
    if (max_level < lvl + lvl / 2)  // 技能太烂
    {
        max_level  = lvl + lvl / 2;
        base_skill = "sword";  // 使用自带的剑法
    }

    // 空手技能除外
    if (base_skill == "unarmed" || base_skill == "hand"
        || base_skill == "finger" || base_skill == "leg" || base_skill == "cuff"
        || base_skill == "claw" || base_skill == "strike")
        return 0;

    CContainer* weapon = NULL;
    // 根据技能给一把武器
    weapon = g_item_mgr->load_item("mujian");

    weapon->set_name("木刺");
    weapon->set("skill_type", base_skill.c_str());
    weapon->set_weight(100);

    weapon->set("value", 1);
    weapon->set("rigidity", 1000);
    weapon->set("material", "steel");
    weapon->set("wield_msg", "$N掣出一柄$n握在手中。");
    weapon->set("unwield_msg", "$N将手中的$n反别腰后。");
    weapon->set("apply/damage", 1);
    weapon->set("owner", "monster");
    return weapon;
};

virtual void catch_ob(CChar* me) {}

char* chat_msg()
{
    // 检查是否该逃走了。
    if (g_utils.random(10)) return 0;

    message_vision("$N身形晃了几晃，眨眼间就消失了！", this);
    DESTRUCT_OB(this, "monster::chat_msg");
    return 0;
}
NPC_END;

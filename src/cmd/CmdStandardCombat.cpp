#include "CmdStandard.h"
#include "../skill/Skill.h"
#include "../room/Room.h"
#include "../item/FightRoom.h"
#include "../npc/Npc.h"
#include "../combat/CombatRules.h"

int CCmdStandard::kill(CChar* me, int32_t target, char* arg)  // 记仇的
{
    CChar* who;
    CContainer* env = me->environment();

    if (!(who = get_room_char(me, target))) return 0;

    if (userp(me) && me->querystr_temp("party/master")
        && me->querystr_temp("party/master")[0]
        && me->query_temp("party/follow")) {
        CChar* pary_master
            = (CChar*)find_online(me->querystr_temp("party/master"));
        if (pary_master && env->present(pary_master->id(1), 1)) {
            return notify_fail("你正处于组队跟随模式！");
        }
    }

    if (userp(me) && userp(who)) {
        // 出手准入：任一数据集规则反对即拒绝（规则内容与归属全在 Lua）
        std::string gate = combat_rule_reason("attack_gate", me, who);
        if (!gate.empty()) return notify_fail("%s", gate.c_str());

        if (who->query_temp("fight/is_fighting")) {
            return notify_fail("不能在观战中暗中偷袭。");
        }
        if (who->query_temp("保镖")) {
            return notify_fail(
                "你看着众多的趟子手簇拥在他一旁，手一软，不敢再下手。");
        }
    }

    if (me->cmd_hit_ob(who, arg)) {
        me->set_killer(who);
        who->set_killer(me);

        if (env->query("city") && userp(who)) {
            // 守卫反应：任一数据集规则命中则豁免召唤（如受害者已在名单中）
            if (!combat_rule_yes("guard_response", me, who)) {
                CNpc* guard = load_npc("pub_guard");
                guard->move(env);
                guard->set("combat_exp", int(me->query_exp() * 1.5));
                guard->set("guard", atoi(who->id(1)));
            }
        }
        if (userp(me) && userp(who)) static_cast<CUser*>(me)->ready_pk();
        return 1;
    }

    return 0;
}

int CCmdStandard::cast(CChar* me, int32_t target, char* arg)
{
    const char* skill;
    CSkill* sk;
    CContainer* who = NULL;
    char magic[40];

    if (!(who = get_room_object(me, target))) who = get_self_object(me, target);

    if (me->is_busy()) return notify_fail("你上一个动作还没有完成，不能施法。");

    if (((me->environment())->query("no_cast")
         || (me->environment())->query("no_all"))
        && !wizardp(me))
        return notify_fail("这里禁止使用法术！");

    if (me->query_temp("no_cast")) return notify_fail("你不能立刻使用法术!");

    analyse_string(arg, magic, 40);

    // 判断角色是否掌握该技能。
    skill = arg;
    if (me->query_skill(skill, 1) < 1) return notify_fail("你还不会这种技能！");

    sk = load_skill(skill);

    // 也有例外，如果技能是"special"类型可以不装备直接使用。
    if (!sk->query("special")) {
        skill = me->query_skill_mapped(arg);

        if (!strlen(skill)) {
            return notify_fail("请先准备技能。");
        }

        sk = load_skill(skill);

        char valid_types[7][10]
            = {"unarmed", "finger", "hand", "cuff", "claw", "strike", "leg"};
        CContainer* weapon = me->query_weapon();

        if (DIFFERSTR(arg, "force"))  // 非内功，则要判断武器
        {
            int can_use = 0;
            for (int i = 0; i < 7; i++) {
                if (strcmp(arg, valid_types[i]) == 0) {
                    // 检查武器
                    if (!weapon
                        || DIFFERSTR(weapon->querystr("skill_type"),
                                     valid_types[i]))
                        can_use = 1;
                }
            }

            if (can_use && weapon) return notify_fail("你必须空手！");

            if (!can_use
                && (DIFFERSTR(arg, "parry") && DIFFERSTR(arg, "dodge")
                    && DIFFERSTR(arg, "throwing"))
                && (!weapon || DIFFERSTR(weapon->querystr("skill_type"), arg)))
                return notify_fail("使用的武器不对！");
        }
    }

    notify_fail("你所使用的技能不支持该法术。");
    if (sk->cast(magic, me, who)) {
        // 检查who 是否存在
        if (g_last_destruct_object == target) who = 0;

        me->improve_skill(skill, 1 + me->query("level") / 5, 1);

        // PK 敌意判定：法术是否构成动手取决于 Lua 声明的 hostile 标志。
        // 非敌意（治疗/增益/自身调理）不挂标记；敌意且波及玩家受害者、
        // 或无需选定受害者的全场技（狮子吼一类）挂标记。
        if (sk->query_magic_hostile(magic)
            && ((who && userp(who) && who != me)
                || sk->query_magic_target(magic) == CSkill::CAST_TO_NONE)) {
            if (userp(me)) {
                static_cast<CUser*>(me)->ready_pk();
            }
        }

        return 1;
    }

    return 0;
}

// 加入战斗 join 12 1

int CCmdStandard::join_battle(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_join的场景使用JoinBattle命令
    if (((me->environment())->query("no_join")
         || (me->environment())->query("no_all"))
        && !wizardp(me))
        return notify_fail("这里禁止加入战斗");

    int team;
    CFightRoom* obj;
    CContainer* who;
    char msg[512];

    if (me->is_busy()) return notify_fail("你现在正忙。");

    // 要求是战斗对象
    if (!(who = get_room_object(me, target))) return 0;
    if (!who->query_attr(CChar::IS_FIGHTROOM)) return 0;
    if (!atoi(arg))  // 加入A队
        team = CFightRoom::FIGHT_TEAM_A;
    else
        team = CFightRoom::FIGHT_TEAM_B;

    obj = static_cast<CFightRoom*>(who);
    if (!obj->can_join(team, me)) {
        DTItemList* list = obj->get_item_list();
        for (auto val : *list) {
            CContainer* obj_smash = val;
            if (!obj_smash->is_character()) continue;
            CChar* target = static_cast<CChar*>(obj_smash);
            if (!userp(target)) continue;
            if ((target->query_temp("fight/is_fighting")
                 && target->query_temp("net_dead"))) {
                obj->set("smash", 1);
                break;
            }
        }
        if (obj->querystr("lock_msg")[0] && !obj->query("smash"))
            return notify_fail("%s", obj->querystr("lock_msg"));

        if (!obj->query("smash")) return notify_fail("此战斗不能加入!");
    }
    // 禁止加入NPC一方进行战斗：数据集规则可声明豁免对象
    int team_obj     = 100;
    DTItemList* list = obj->get_item_list();
    for (auto val : *list) {
        CContainer* obj1 = val;
        if (!obj1->is_character()) continue;
        CChar* target = static_cast<CChar*>(obj1);
        if (!userp(target) && !target->query("help")
            && !combat_rule_yes("join_battle", me, target)) {
            team_obj = target->query_temp("fight/fight_team");
            break;
        }
    }
    if (team_obj != 100 && team == team_obj && userp(me)
        && !obj->query("smash")) {
        return notify_fail("此战斗禁止加入NPC一方!");
    }
    else {
        me->move(obj);
        obj->join_team(me, team);

        tell_room(obj,
                  snprintf(msg, 255, "%s加入%s.", me->name(), obj->name()));
        tell_room(me->environment(),
                  snprintf(msg, 255, "%s加入战队%d.", me->name(), team));

        // PK Mark!
        if (userp(me)) static_cast<CUser*>(me)->ready_pk();
    }
    return 1;
}

// 观战

int CCmdStandard::guard(CChar* me, int32_t target, char* arg)
{
    CContainer* obj;
    char msg[255];

    if (me->is_busy()) return notify_fail("你现在正忙。");

    // 要求是战斗对象
    if (!(obj = get_room_object(me, target))) return 0;
    if (!obj->query_attr(CChar::IS_FIGHTROOM)) return 0;

    if (!wizardp(me)) {
        // 观战准入：战斗声明 no_guard 即拒绝，文案可选
        const char* reason = obj->querystr("no_guard");
        if (reason[0]) return notify_fail("%s", reason);
        if (obj->query("no_guard")) return notify_fail("此战斗不允许观战!");

        if (obj->query("room_locked") & FIGHT_NO_GUARD)
            return notify_fail("此战斗被锁住，无法观战。");
    }

    me->move(obj);
    message_vision("$N加入观战。", me);
    tell_room(obj->environment(),
              snprintf(msg, 255, "%s观看%s。", me->name(), obj->name()));
    return 1;
}

int CCmdStandard::fight(CChar* me, int32_t target, char* arg)
{
    char msg[255];
    CChar* who;
    CContainer* env = me->environment();

    //----------------------------------------
    // 增加坐标信息
    char szY[40];
    analyse_string(arg, szY, 40);
    int32_t x = atol(arg);
    int32_t y = atol(szY);

    if (x && y) {
        set_live_dest(me, x, y);
    }
    //----------------------------------------

    if ((env->query("no_fight") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止战斗。");

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要和谁切磋？");

    if (userp(me) && me->querystr_temp("party/master")
        && me->querystr_temp("party/master")[0]
        && me->query_temp("party/follow")) {
        CChar* pary_master
            = (CChar*)find_online(me->querystr_temp("party/master"));
        if (pary_master && env->present(pary_master->id(1), 1)) {
            return notify_fail("你正处于组队跟随模式！");
        }
    }

    if (env->query_attr(CChar::IS_FIGHTROOM)) return 1;  // 已经在战斗了。

    if (!living(who)) return notify_fail("%s已经无法战斗了。", who->name());

    if (who == me) return notify_fail("你不能攻击自己。");

    if (who->is_busy()) return notify_fail("对方没空理你。");

    if (userp(who) && who->query_temp("pending/fight") != me->object_id()) {
        message_vision(snprintf(msg, 255, "$N对著$n说道：%s%s，领教%s的高招！",
                                query_self(me), me->name(), query_respect(who)),
                       me, who);

        me->set_temp("pending/fight", who->object_id());
        return 1;
    }

    if (who->query("can_speak")) {
        message_vision(snprintf(msg, 255, "$N对著$n说道：%s%s，领教%s的高招！",
                                query_self(me), me->name(), query_respect(who)),
                       me, who);

        notify_fail("看起来%s并不想跟你较量。", who->name());
        if (who->query_attr(CChar::IS_NPC)
            && !(static_cast<CNpc*>(who))->accept_fight(me))
            return 0;

        snprintf(msg, 255, "%s与%s的比试", me->name(), who->name());
        me->delete_temp("pending/fight");
        who->delete_temp("pending/fight");
        do_bishi(me, who, msg, 50);
    }
    else {
        message_vision("$N大喝一声，开始对$n发动攻击！", me, who);
        who->kill_ob(me);
    }

    return 1;
}

int CCmdStandard::follow(CChar* me, int32_t target, char* arg)
{
    // 禁止在设置为no_follow的场景使用Follow命令
    CContainer* env = me->environment();
    if ((env->query("no_follow") || env->query("no_all")) && !wizardp(me))
        return notify_fail("这里禁止跟随他人。");

    CChar* who;

    if (!(who = get_room_char(me, target))) return notify_fail("你要跟随谁？");

    if (who == me) {
        me->set_leader(0);
        tell_object(me, "取消跟随！");
        return 1;
    }

    me->set_leader(who);
    message_vision("$YEL$N决定开始跟随$n一起行动。", me, who);
    return 1;
}

int CCmdStandard::halt(CChar* me, int32_t target, char* arg)
{
    char msg[255];

    if ((me->environment())->query_attr(CChar::IS_FIGHTROOM)) {
        CFightRoom* env = (CFightRoom*)me->environment();
        if (me->is_fighting()) {
            if (env->query("room_locked") & FIGHT_NO_FLEE)  // 被锁死
                return notify_fail("该战斗被锁住，无法逃脱。");

            if (env->is_threshold_match()) {
                message_vision("$N身形向后一跃，跳出战圈不打了。", me);
                env->end_fight();
            }
            else {
                // busy中不能逃跑
                if (me->is_busy()) return notify_fail("你没空逃跑！");

                if (!check_ap(50, me)) return 0;
                if (env->do_flee(me)) resume_ap(50, me);
            }
        }
        else  // 观战
        {
            me->move(env->environment());
            tell_room(env, snprintf(msg, 255, "%s停止观战。", me->name()));
            message_vision(snprintf(msg, 255, "$N离开了%s。", env->name()), me);
        }

        return 1;
    }
    else if (!me->is_busy())
        return notify_fail("你现在不忙。");

    me->interrupt_me(me, "halt");
    return 1;
}

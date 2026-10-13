/**
 * @file House.h
 * @brief 玩家私人房间。
 */

#pragma once

int valid_string(const char* msg);

#include "Resident.h"
#include "Channel.h"
#include "Npc.h"
#include "Combatd.h"
#include "CombatConfig.h"

ROOM_BEGIN(CHouse);

void create()
{
    if (_M.undefinedp("name")) set_name("房间", "house");

    set("sleep_room", 1);
    set("area", "house");

    set("saveobjects", 1);
    set("max_items", 20);

    CBox* box = add_box();
    box->set_name("柜子", "box");
    box->set("max_items", 50);
    box->set("no_drop_man", 1);  // 箱子里不可以装人

    int count = query("box_count");
    for (int i = 1; i < count; i++) {
        CBox* box = add_box();
        char name[10];
        box->set_name(snprintf(name, 10, "柜子%d", i + 1), "box");
        box->set("max_items", 50);
        box->set("no_drop_man", 1);  // 箱子里不可以装人
    }

    make_yongren(querymap("yongren"));
}

void setup()
{
    CDoor* door = add_door("出口", querystr("belong"), querystr("name"), "out");
    door->set_name(querystr("belong"));

    if (query("size") < 2)
        set("map", "玩家民居1");
    else
        set("map", "玩家民居2");

    if (!query("fail_ob")) check_pays();
}

static void remove_me(CContainer* ob, int32_t param1, int32_t param2)
{
    DESTRUCT_OB(ob, "CHouse::remove_me");
}

void reset()
{
    if (!g_combat_config.no_check_house()) {
        if (!load())  // 此房间已经不存在
        {
            call_out(remove_me, 1);
            return;
        }
    }

    CNpc* npc = find_npc(query_temp("yongren"));
    if (!npc) make_yongren(querymap("yongren"));

    // 检查拖欠的管理费
    check_pays();
}

void make_yongren(CMapping* yongren)
{
    CNpc* npc = load_npc("monster_yongren");

    if (yongren) {
        npc->set_name(yongren->querystr("name"));
        npc->set("welcome", yongren->querystr("welcome"));
        npc->set("long", yongren->querystr("long"));
        npc->set("welcome_master", yongren->querystr("welcome_master"));
        npc->set("combat_exp", yongren->query("combat_exp"));
    }

    if (!npc->query_exp()) npc->set("combat_exp", 20000);

    setup_char(npc);

    npc->move(this);

    set_temp("yongren", npc->object_id());
}

int32_t query_attr(int attr)
{
    if (attr == CAN_SAVE) return 1;

    return CRoom::query_attr(attr);
}

int handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    CChar* yongren = (CChar*)present_name("monster_yongren", IS_NPC);
    char msg[255];

    // 任何时候都不可以使用roar
    if (yongren && strcmp(comm, "cast") == 0
        && strcmp(arg, "force roar") == 0) {
        message_vision("$N拦住$n道：“想抢书？出去！”", yongren, me);
        return 1;
    }

    // 不是主人
    if (DIFFERSTR(querystr("owner"), me->id(1))
        && DIFFERSTR(querystr("second_owner"), me->id(1))) {
        if (yongren) {
            // 截获使用时空贴的指令！
            if (strcmp(comm, "trans") == 0 || strcmp(comm, "use") == 0) {
                CContainer* obj = me->present(atol(tar));
                if (!obj) return 0;

                if (EQUALSTR(obj->querystr("base_name"), "transfer_tie")
                    || EQUALSTR(obj->querystr("base_name"),
                                "huicheng_juanzhou")) {
                    if (check_rent(me)) {
                        message_vision(
                            "$N偷偷拿出$"
                            "n，结果被仆人拦住：“请把借的东西留下！”",
                            me, obj);
                        return 1;
                    }
                }
            }

            // 截获hit，kill，fight，cast，join的指令
            if (strcmp(comm, "kill") == 0 || strcmp(comm, "fight") == 0
                || strcmp(comm, "cast") == 0 || strcmp(comm, "join") == 0
                || strcmp(comm, "guard") == 0) {
                CContainer* obj = present(atol(tar));
                if (!obj) return 0;

                if (check_rent(me) || check_rent((CChar*)obj)) {
                    message_vision("$N拦住$n道：“不行，想搞什么出去搞！”",
                                   yongren, me);
                    return 1;
                }
            }

            // 截获drop指令
            if (strcmp(comm, "drop") == 0) {
                CContainer* obj = me->present(atol(tar));
                if (!obj) return 0;

                if (obj->query_temp("yongren/rent")) {
                    message_vision("$N偷偷摸摸的拿出$n，想丢在地上。", me, obj);
                    message_vision("$N见状喝道：想干什么！不想要的话还给我！",
                                   yongren, me);
                    return 1;
                }
            }

            // 截获give指令
            if (strcmp(comm, "give") == 0) {
                CContainer* obj = me->present(atol(tar));
                if (!obj) return 0;

                // 寻找目标
                int32_t target = 0;

                if (sscanf(arg, "to %ld", &target) != 1) return 0;

                CContainer* who = present(target);
                if (who == yongren) return 0;  // 可以给用人

                if (obj->query_temp("yongren/rent")) {
                    message_vision("$N偷偷摸摸的拿出$n，想转手给人。", me, obj);
                    message_vision("$N见状喝道：想干什么！不想要的话还给我！",
                                   yongren, me);
                    return 1;
                }
            }

            // 截获get指令
            if (strcmp(comm, "get") == 0) {
                // 判断物品是否合法
                CContainer* obj = present(atol(tar));
                if (!obj) return 0;

                if (obj->query_temp("yongren/rent")) {
                    message_vision(
                        snprintf(msg, 255,
                                 "$N弯腰去拣%s，结果被$n一脚踩到了手指上！",
                                 obj->name()),
                        me, yongren);
                    return 1;
                }

                if (obj->is_character() && check_rent((CChar*)obj)) {
                    message_vision("$N忽然拦住$n，笑道：还是算了吧！", yongren,
                                   me);
                    return 1;
                }
                if (userp(obj)) {
                    message_vision("$N疑惑的问着$n：你想拣什么？", yongren, me);
                    return 1;
                }
            }

            // 截获sell指令
            if (strcmp(comm, "sell") == 0) {
                // 判断是否能卖
                CContainer* obj = me->present(atol(arg));
                if (!obj) return 0;

                if (obj->query_temp("yongren/rent")) {
                    message_vision(
                        snprintf(msg, 255,
                                 "$N想卖掉%s，结果被$n伸手拦住：借的东西快还！",
                                 obj->name()),
                        me, yongren);
                    return 1;
                }
            }
        }
    }
    else {
        if (strcmp(comm, "housename") == 0)
            return housename(me, arg);
        else if (strcmp(comm, "housedesc") == 0)
            return housedesc(me, arg);
        else if (strcmp(comm, "addmaster") == 0)
            return addmaster(me, arg);
        else if (strcmp(comm, "kickout") == 0)
            return do_kick(me, arg);
        else if (strcmp(comm, "paymoney") == 0)
            return do_paymoney(me, arg);
    }

    return CRoom::handle_action(comm, me, arg, tar);
}

CContainer* check_rent(CChar* me)
{
    if (EQUALSTR(querystr("owner"), me->id(1))
        || EQUALSTR(querystr("second_owner"), me->id(1)))
        return 0;

    // 搜身
    DTItemList* list = me->get_item_list();

    for (auto obj : *list) {
        if (obj->query_temp("yongren/rent")) return obj;
    }

    return 0;
}

int valid_leave(CChar* me, const char* dir)
{
    // 租借的物品不允许带出房间
    if (EQUALSTR(dir, "出口")) {
        CChar* who      = (CChar*)present_name("monster_yongren", IS_NPC);
        CContainer* obj = check_rent(me);
        if (who && obj)
            return notify_fail("%s伸手拦住你，说：把%s留下！！", who->name(),
                               obj->name());
    }

    return CRoom::valid_leave(me, dir);
}

// 设置第二房主
int addmaster(CChar* me, char* arg)
{
    if (DIFFERSTR(querystr("owner"), me->id(1))) {
        say("对不起，只有户主才邀请其他人同住。", me);
        send_menu(me);
        return 1;
    }

    if (query("size") < 2) {
        say("对不起，这个房间不能再住其他人了。", me);
        send_menu(me);
        return 1;
    }

    CUser* owner = find_online(arg);
    if (!owner) {
        say("对不起，请这个人在线时再来注册房主。", me);
        send_menu(me);
        return 1;
    }

    set("second_owner", arg);
    update_register();

    tell_object(me, "OK.");
    return 1;
}

// 赶出去
int do_kick(CChar* me, char* arg)
{
    char msg[255];
    CUser* who = find_online(arg);
    if (!who) return notify_fail("你想踢谁？");

    if (EQUALSTR(querystr("owner"), who->id(1)))
        return notify_fail("没搞错，人家可是房主哎！！");

    if (who->environment() == this) {
        message_vision("$HIY$N飞起一脚，将$n踢出了门外！", me, who);

        // 先把身上的租用品留下！
        DTItemList list;

        copy_list(&list, who->get_item_list());

        for (auto obj : list) {
            if (obj->query_temp("yongren/rent")) {
                obj->move(this);
                message_vision("$n从$N身上掉落下来。", who, obj);
            }
        }

        list.clear();

        who->move(load_room(querystr("belong")));
    }
    else {
        tell_object(me,
                    snprintf(msg, 255, "你拒绝让%s登门造访！", who->name(1)));
    }

    if (EQUALSTR(who->querystr_temp("invite"), querystr("owner")))
        who->delete_temp("invite");

    return 1;
}

// 设置房间名称
int housename(CChar* me, char* arg)
{
    string msg;

    msg = check_legal_name(arg);
    if (msg.length()) {
        say(msg.c_str(), me);
        send_menu(me);
        return 1;
    }

    if (!lock_data(1)) return 0;

    if (!load())  // 此房间已经不存在
    {
        call_out(remove_me, 1);
        return 1;
    }

    set_name(arg);

    update_register();
    lock_data(0);

    tell_object(me, "OK.");

    return 1;
}

// 设置房间描述
int housedesc(CChar* me, char* arg)
{
    int len = strlen(arg);
    if (len < 1 || len > 500) {
        say("给您的房间描述写得简洁漂亮点。", me);
        send_menu(me);
        return 1;
    }

    if (!valid_string(arg)) return 0;

    if (!lock_data(1)) return 0;

    if (!load())  // 此房间已经不存在
    {
        call_out(remove_me, 1);
        return 1;
    }

    set("desc", arg);

    save();
    lock_data(0);

    return 1;
}

// 更新注册信息
void update_register()
{
    char msg[255];
    CRoom* r = load_room(querystr("belong"));

    if (!r->query_attr(IS_RESIDENT)) {
        g_channel.do_channel(
            &g_channel, 0, "sys",
            snprintf(msg, 255,
                     "update:房间%s所注册的区域%s不是可利用的居住区！", name(1),
                     r->name(1)));
        return;
    }

    ((CResident*)r)->update_register(this);

    save();
}

virtual int do_look(CChar* me)
{
    say(long_desc(), me);
    say(querystr("desc"), me);

    if (EQUALSTR(querystr("owner"), me->id(1))
        || EQUALSTR(querystr("second_owner"), me->id(1))) {
        add_menu_item("拒绝客人来访", "$2kickout 0 $2", me);
        add_menu_item("修改房间名称", "$2housename 0 $2", me);
        add_menu_item("修改房间描述", "$2housedesc 0 $2", me);

        if (EQUALSTR(querystr("owner"), me->id(1)) && query("size") > 1)
            add_menu_item("邀请其他人同住", "$2addmaster 0 $2", me);
    }

    send_menu(me);
    return 1;
}

// 检查拖欠管理费
void check_pays()
{
    char msg[255];
    const char* bl = querystr("belong");
    if (!bl || !bl[0]) return;

    CRoom* r = load_room(bl);

    if (!r->query_attr(IS_RESIDENT)) {
        g_channel.do_channel(
            &g_channel, 0, "sys",
            snprintf(msg, 255,
                     "check_pays:房间%s所注册的区域%s不是可利用的居住区！",
                     name(1), r->name(1)));
        return;
    }

    int days      = ((CResident*)r)->count_days(querystr("owner"));
    int32_t price = ((CResident*)r)->house_price();

    if (query("size") < 2) price /= 2;

    price = days * price / 250;

    if (price <= 0) {
        set("need_pays", 0l);
        return;
    }

    // 自动从银行扣除房租
    CUser* owner = find_online(querystr("owner"));
    int flag1    = 0;
    if (!owner) {
        flag1 = 1;
        owner = add_user();
        owner->set("id", querystr("owner"));
        owner->load_from_server();
    }
    if (owner->query("balance") >= price) {
        owner->add_balance(-price);
        del("need_pays");
        set("pay_time", g_current_time);
        update_register();
        owner->save();
        if (flag1) DESTRUCT_OB(owner, "CResident::update_register");
        return;
    }

    if (flag1) DESTRUCT_OB(owner, "CResident::update_register");
    // 自动从银行扣除房租
    set("need_pays", price);
}

// 交纳帐单
// 交纳房屋帐单
int do_paymoney(CChar* me, char* arg)
{
    char msg[255];
    int32_t pay = query("need_pays");
    if (!pay) return 0;

    if (!strlen(arg)) {
        say(snprintf(msg, 255,
                     "一张%s的管理费用结算单。\n\n截止今日累计费用合计%"
                     "ld金。\n\n注意：当拖欠时间超过一个月，您将失去本房间的居"
                     "住权。",
                     name(), pay),
            me, "房屋帐单");
        add_menu_item("结算", "$0paymoney 0 1", me);
        add_menu_item("不予理会", "", me);
    }
    else {
        if (!lock_data(1)) return 0;

        if (!load())  // 此房间已经不存在
        {
            call_out(remove_me, 1);
            return 0;
        }

        check_pays();
        pay = query("need_pays");

        if (!CMoney::player_pay(me, pay)) {
            say("对不起，现金不足！", me);
        }
        else {
            del("need_pays");
            set("pay_time", g_current_time);
            update_register();
        }

        lock_data(0);
        say(snprintf(msg, 255, "支付了%ld金结算帐单！", pay), me);
    }

    send_menu(me);
    return 1;
}

ROOM_END;

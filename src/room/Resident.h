/**
 * @file Resident.h
 * @brief 居民区房间，支持房屋管理/租户/追杀等逻辑。
 */

#pragma once

#include "Room.h"
#include "User.h"
#include "Money.h"
#include "Channel.h"
#include "NpcMirror.h"
#include "Utils.h"

char* check_legal_name(const char* name);

#define RESIDENT_BEGIN(room_name)        \
    class room_name : public CResident { \
    public:                              \
        room_name() {};                  \
        virtual ~room_name() {};
#define RESIDENT_END \
    }                \
    ;

/*

每户资料给0.1K空间 7K
最大可居住70户。全国有15处住宅，也仅能住1050户。
因此，为节约资源：规定，每户房产每天缴纳地价的%1。

上次付款时间。在居民区reset的时候会自动计算一次。!
CUtils::random(10)的时候。不需要每次都算。 当前时间 - 上次付款时间 每天 *
当地房价 / 100; 100W的房子，一周要支付7万的税金。
如果欠费超过房价的一半，就要强制回收。 大约拖欠7周时间，房子就自动回收了。

房主每次回家，如拖欠税金，要有提示。由仆人自动问候的时候说。每次上线只说一次。
房主可与仆人对话查看帐单和交纳税金。

卖房只能卖房价的一半 - 拖欠税金。

*/

ROOM_BEGIN(CResident);

// 计算拖欠天数
int count_days(const char* owner)
{
    char msg[80];

    CMapping* r = querymap(snprintf(msg, 80, "house/%s", owner));
    if (!r) return 0;
    return (g_current_time - r->query("time")) / 86400;
}

// 计算税金
void clear_house()
{
    char msg[255];

    CMapping* house = querymap("house");
    if (!house) return;

    CMapping* r;
    CRoom* room;

    CVector v;

    house->keys(v);
    for (int i = 0; i < v.count(); i++) {
        const char* key = v[i];
        r               = house->querymap(key);
        if (!r) continue;

        int days = count_days(key);

        if (days >= 30)  // 超过30天，房子被自动吊销
        {
            room = get_local_house(key);

            if (!room)  // 该房间已不存在
            {
                set_house(key, 0);
                DESTRUCT_OB(room, "CResident::clear_house");
                return;
            }

            // 自动从银行扣除房租
            int32_t price = house_price();
            if (room->query("size") < 2) price /= 2;
            price        = days * price / 250;
            CUser* owner = find_online(key);
            int flag1    = 0;
            if (!owner) {
                flag1 = 1;
                owner = add_user();
                owner->set("id", room->querystr("owner"));
                owner->load_from_server();
            }

            if (owner->query("balance") > -2000000000) {
                // 持续扣钱，但是不收回房子。再买房子要交双份的钱
                owner->add_balance(-price);
            }
            if (owner->query("balance") >= price) {
                room->del("need_pays");
                room->set("pay_time", g_current_time);
                update_register(room);
                owner->save();
                if (flag1) DESTRUCT_OB(owner, "CResident::update_register");
                return;
            }

            if (flag1) DESTRUCT_OB(owner, "CResident::update_register");

            // 自动从银行扣除房租
            snprintf(msg, 255,
                     "%s由于房主%s(%s)长期拖欠物业管理费，被%s政府抵押收回。",
                     r->querystr("name"), r->querystr("short"), key,
                     _M.querystr("resident"));

            g_channel.do_channel(&g_channel, 0, "news", msg);
            return;  // 必须返回.因为物件销毁,链表出错.一次只收回一座房屋.
        }
    }
}

// 删掉一处住房
void remove_house(CRoom* house)
{
    char msg[255];
    CUser* owner
        = find_online(snprintf(msg, 255, "%s", house->querystr("owner")));
    int flag1 = 0;

    if (!owner) {
        flag1 = 1;
        owner = add_user();

        owner->set("id", house->querystr("owner"));
        owner->load_from_server();
    }

    owner->del("resident");
    owner->set("start_city", g_start_room);
    owner->save();

    if (flag1) DESTRUCT_OB(owner, "CResident::remove_house");

    set_house(house->querystr("owner"), 0);

    house->remove_all_box();
    house->remove();
    DESTRUCT_OB(house, "CResident::remove_house");
}

void reset()
{
    CRoom::reset();

    if (wait_clear--) return;

    load();

    wait_clear = 40 + CUtils::random(40);
    clear_house();
}

// 房产价格
int32_t house_price()
{
    static int price[15] = {
        50,   // 0
        60,   // 5
        72,   // 10
        85,   // 15
        100,  // 20
        125,  // 25
        150,  // 30
        180,  // 35
        216,  // 40
        260,  // 45
        312,  // 50
        375,  // 55
        450,  // 60
        540,  // 65
        650,  // 70
    };

    CMapping* house = querymap("house");
    if (!house) return 500000;

    int lv = house->get_count() / 5;

    if (lv > 14) return 6500000 * 2;
    return price[lv] * 10000;
}

void check_second_owner(CChar* me)  // 增加第二房主的菜单
{
    char msg[255], tmp[80];

    CMapping* house = querymap("house");
    if (!house) return;

    CMapping* r;
    const char* owner = me->id(1);

    CVector v;

    house->keys(v);

    for (int i = 0; i < v.count(); i++) {
        const char* key = v[i];
        r               = house->querymap(key);

        if (strcmp(r->querystr("owner"), owner) == 0
            && strcmp(key, owner) != 0) {
            add_menu_item(snprintf(msg, 255, "进入%s", r->querystr("name")),
                          snprintf(tmp, 80, "$0enter 0 %s", key), me);
        }
    }
}

int do_look(CChar* me)
{
    char msg[255], master[40];
    CMapping* house = querymap("house");

    if (house)
        snprintf(msg, 255, "这里是%s城的住宅区。目前这里有%ld家住户。",
                 querystr("resident"), house->get_count());
    else {
        snprintf(msg, 255, "这里是%s城的住宅区。目前这里没有任何住户。",
                 querystr("resident"));
    }

    say(msg, me);

    if (EQUALSTR(me->querystr("resident"), querystr("resident")))
        add_menu_item("回家", snprintf(msg, 255, "$0enter 0 %s", me->id(1)),
                      me);
    else if (can_apply_house())
        add_menu_item(
            snprintf(msg, 255, "申请成为%s居民", querystr("resident")),
            "$0applyhouse", me);

    check_second_owner(me);  // 增加第二房主的菜单

    if (me->querystr_temp("invite")[0]) {
        strncpy(master, me->querystr_temp("invite"), 40);
        CChar* u = find_online(master);
        if (u)
            add_menu_item(snprintf(msg, 255, "去%s的家", u->name()),
                          snprintf(master, 40, "$0enter 0 %s", u->id(1)), me);
    }

    if (house) add_menu_item("查看有哪些住户", "$0checkhouse", me);

    if (EQUALSTR(me->querystr("resident"), querystr("resident"))) {
        CRoom* r = get_my_house(me);
        if (r)
            add_menu_item(snprintf(msg, 255, "卖掉%s", r->name()),
                          "$0sellhouse 0 0", me);
    }

    send_menu(me);
    return 1;
}

// 是否可以申请住房
int can_apply_house()
{
    CMapping* house = querymap("house");
    if (!house) return 1;
    if (house->get_count() < 70) return 1;
    return 0;
}

int handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (strcmp(comm, "enter") == 0)
        return backhome(me, arg);
    else if (strcmp(comm, "applyhouse") == 0)
        return applyhouse(me);
    else if (strcmp(comm, "applyroom") == 0)
        return applyroom(me, arg);
    else if (strcmp(comm, "housename") == 0)
        return housename(me, arg);
    else if (strcmp(comm, "housedesc") == 0)
        return housedesc(me, arg);
    else if (strcmp(comm, "registerhouse") == 0)
        return registerhouse(me);
    else if (strcmp(comm, "addmaster") == 0)
        return addmaster(me, arg);
    else if (strcmp(comm, "checkhouse") == 0)
        return checkhouse(me);
    else if (strcmp(comm, "sellhouse") == 0)
        return sellhouse(me, arg);

    return CRoom::handle_action(comm, me, arg, tar);
}

// 针对房间列表的修改均在此
int set_house(const char* menpai, CMapping* m)
{
    char msg[80];
    if (!lock_data(1)) return 0;
    load();

    if (m) {
        set(snprintf(msg, 80, "house/%s", menpai), (*m));
    }
    else {
        del(snprintf(msg, 80, "house/%s", menpai));
    }

    save();
    lock_data(0);
    return 1;
}

// 卖掉自己的住房
int sellhouse(CChar* me, const char* arg)
{
    char msg[255];

    // 判断房间是否存在本qu
    if (!querymap(snprintf(msg, 255, "house/%s", me->id(1)))) {
        say("你要卖掉的房间不在本区！", me);
        send_menu(me);
        return 1;
    }

    CRoom* house = get_my_house(me);

    if (!house || !house->load()) {
        DESTRUCT_OB(house, "CResident::sellhouse");
        return notify_fail("你的房子已经卖掉了．");
    }

    if (DIFFERSTR(house->querystr("owner"), me->querystr("id")))
        return notify_fail("这不是你的房子。");

    int32_t price = house_price();
    if (house->query("size") == 1) price /= 2;  // 单人房的折半

    price /= 2;  // 卖房只值原价的一半

    int32_t shui = price * count_days(me->querystr("id")) / 500;
    price -= shui;

    if (price < 0) price = 1;

    if (EQUALSTR(arg, "0")) {
        say(snprintf(msg, 255,
                     "%s目前价值%ld。卖掉房子意味着你从本市迁出。真的要卖吗？",
                     house->name(), price),
            me);
        add_menu_item("决定卖了", "$0sellhouse 0 1", me);
        add_menu_item("离开", "", me);
        send_menu(me);
        return 1;
    }

    say(snprintf(msg, 255, "你卖掉了%s，获得%ld。", house->name(), price), me);
    send_menu(me);

    // 真的卖了。
    CMoney::pay_player(me, price);

    // 写入日志
    LOG_INFO("%s(%s)在%s卖掉房子%s(%s)，获利%ld。From (%s)", me->name(1),
             me->id(1), name(1), house->name(1), house->querystr("base_name"),
             price, get_ip(me));

    remove_house(house);
    return 1;
}

// 返回玩家的房间 这是有可能返回0的函式。0表示玩家尚无住宅。
static CRoom* get_my_house(CChar* me)
{
    char msg[255];

    if (!me->querystr("resident")[0]) return 0;
    CRoom* r = load_room(
        snprintf(msg, 255, "house_%s_%s", me->querystr("resident"), me->id(1)));
    if (r->query("fail_ob")) return 0;
    return r;
}

CRoom* get_local_house(const char* menpai, int force = 0)
{
    char msg[255];
    CRoom* r = load_room(
        snprintf(msg, 255, "house_%s_%s", querystr("resident"), menpai));
    if (!force && !r->load()) return 0;
    return r;
}

int backhome(CChar* me, char* arg)
{
    char msg[255];

    // 判断要闯入的房间是否存在
    CMapping* house = querymap(snprintf(msg, 255, "house/%s", arg));

    if (!house) {
        say("你要进入的房间不在本区！", me);
        send_menu(me);
        return 1;
    }

    if (EQUALSTR(arg, me->querystr("id"))
        || EQUALSTR(house->querystr("owner"), me->querystr("id"))
        || EQUALSTR(arg, me->querystr_temp("invite"))) {
        CRoom* r = get_local_house(arg);
        if (!r) {
            say("此房间不存在!", me);
            send_menu(me);
            return 1;
        }

        if (DIFFERSTR(arg, me->querystr("id"))
            && DIFFERSTR(house->querystr("owner"), me->querystr("id"))) {
            if (!r->present(arg)) {
                say("主人不在家，不能闯入！", me);
                send_menu(me);
                return 1;
            }
        }
        message_vision_room(me, "chat",
                            snprintf(msg, 255, "%s进入了%s。", me->name(),
                                     house->querystr("name")),
                            me);
        me->move(r);
        message_vision_room(me, "chat",
                            snprintf(msg, 255, "%s走了进来。", me->name()), me);
        return 1;
    }

    say(snprintf(msg, 255, "你试图闯入%s，结果没有成功！",
                 house->querystr("name")),
        me);
    send_menu(me);
    return 1;
}

int applyhouse(CChar* me)
{
    char msg[255];

    if (me->querystr("resident")[0]) {
        say(snprintf(msg, 255, "你已经申请成为%s人了。",
                     me->querystr("resident")),
            me);
        send_menu(me);
        return 1;
    }

    if (me->query("age") < 16) {
        say(snprintf(msg, 255, "你还没成年，做不了房主。"), me);
        send_menu(me);
        return 1;
    }

    add_menu_item("申请单人间", "$0applyroom 0 1", me);
    add_menu_item("申请双人套房", "$0applyroom 0 2", me);  // 可以有两个主人
    send_menu(me);
    return 1;
}

int32_t need_money(CChar* me)
{
    int32_t money = house_price();

    if (me->query_temp("house/size") == 1) money /= 2;

    return money;
}

int applyroom(CChar* me, const char* arg)
{
    char msg[255];
    CContainer* coin = me->present("coin");

    me->set_temp("house/size", 2);
    if (EQUALSTR(arg, "1")) {
        me->set_temp("house/size", 1);
    }

    int32_t money = need_money(me);

    if (!coin || coin->query("value") < money) {
        say(snprintf(msg, 255, "申请房间需要%ld金，请备好现金再来。", money),
            me);
        send_menu(me);
        return 1;
    }

    menulist(me);
    return 1;
}

void menulist(CChar* me)
{
    add_menu_item("设置房间名称", "$2housename 0 $2", me);
    add_menu_item("设置房间描述", "$2housedesc 0 $2", me);

    if (me->query_temp("house/size") > 1)
        add_menu_item("设置第二房主", "$2addmaster 0 $2", me);

    if (me->querystr_temp("house/name")[0]) {
        add_menu_item("申请房间", "$0registerhouse", me);
    }

    send_menu(me);
}

// 设置第二房主
int addmaster(CChar* me, char* arg)
{
    CUser* owner = find_online(arg);
    if (!owner) {
        say("对不起，请这个人在线时再来注册房主。", me);
    }
    else
        me->set_temp("house/owner", arg);

    menulist(me);
    return 1;
}

// 设置房间名称
int housename(CChar* me, char* arg)
{
    const char* msg = check_legal_name(arg);
    if (msg[0]) {
        say(msg, me);
    }
    else
        me->set_temp("house/name", arg);

    menulist(me);
    return 1;
}

// 设置房间描述
int housedesc(CChar* me, char* arg)
{
    int len = strlen(arg);
    if (len < 1 || len > 500) {
        say("给您的房间描述写得简洁漂亮点。", me);
    }
    else
        me->set_temp("house/desc", arg);

    menulist(me);
    return 1;
}

// 更新注册
void update_register(CRoom* house)
{
    CMapping m;
    char msg[255];

    m.set("name", house->querystr("name"));
    m.set("owner", house->querystr("second_owner"));
    m.set("time", house->query("pay_time"));  // 上次付费时间

    CUser* owner
        = find_online(snprintf(msg, 255, "%s", house->querystr("owner")));
    int flag1 = 0;

    if (!owner) {
        flag1 = 1;
        owner = add_user();

        owner->set("id", house->querystr("owner"));
        owner->load_from_server();
    }

    if (house->querystr("second_owner")[0]) {
        CUser* owner2 = find_online(
            snprintf(msg, 255, "%s", house->querystr("second_owner")));
        int flag2 = 0;

        if (!owner2) {
            flag2  = 1;
            owner2 = add_user();
            owner2->set("id", house->querystr("second_owner"));
            owner2->load_from_server();
        }

        house->set("long", snprintf(msg, 255, "这里是%s和%s的共享房间。",
                                    owner->name(1), owner2->name(1)));
        m.set("short",
              snprintf(msg, 255, "%s和%s", owner->name(1), owner2->name(1)));
        if (flag2) DESTRUCT_OB(owner2, "CResident::update_register");
    }
    else {
        house->set("long",
                   snprintf(msg, 255, "这里是%s的私人房间。", owner->name()));
        m.set("short", snprintf(msg, 255, "%s", owner->name(1)));
    }

    if (flag1) DESTRUCT_OB(owner, "CResident::update_register");

    set_house(house->querystr("owner"), &m);
}

// 注册房间
int registerhouse(CChar* me)
{
    char msg[255];

    const char* house_name = me->querystr_temp("house/name");
    if (!house_name[0]) {
        add_menu_item("请给您的房间起个名字", "$1housename 0 $2", me);
        send_menu(me);
        return 1;
    }

    if (!can_apply_house()) {
        say("对不起，本居民区已无空房，不能办理申请手续！", me);
        send_menu(me);
        return 1;
    }

    int32_t money = need_money(me);
    if (!CMoney::player_pay(me, money)) {
        say("对不起，你的现金不足！", me);
        send_menu(me);
        return 1;
    }

    CRoom* r = get_local_house(me->querystr("id"), 1);

    if (!r) {
        say("此房间不存在!", me);
        send_menu(me);
        return 1;
    }

    time_t t;

    r->set_name(me->querystr_temp("house/name"));
    r->set("desc", me->querystr_temp("house/desc"));
    r->set("size", me->query_temp("house/size"));
    r->set("owner", me->id(1));
    r->set("belong", querystr("base_name"));
    r->set("second_owner", me->querystr_temp("house/owner"));
    r->set("pay_time", static_cast<int32_t>(time(&t)));  // 管理费开始时间
    r->del("fail_ob");

    r->remove_all_doors();
    r->setup();

    r->save();

    me->set("resident", querystr("resident"));
    me->delete_temp("house");

    if (EQUALSTR(me->querystr("title"), "普通百姓")) {
        me->set("title", snprintf(msg, 255, "%s百姓", querystr("resident")));
        me->update_me();
    }

    // 设置起始房间
    me->set("start_city", r->querystr("base_name"));
    me->save();

    // 注册
    update_register(r);

    char tmp[40];

    add_menu_item(snprintf(msg, 255, "进入%s。", r->name()),
                  snprintf(tmp, 40, "$0enter 0 %s", me->id(1)), me);

    send_menu(me);

    // 记日志
    LOG_INFO("%s(%s)在%s申请住房%s(%s)，花费%ld。", me->name(1), me->id(1),
             name(1), r->name(1), r->querystr("base_name"), money);

    return 1;
}

// 列出住户名单
int checkhouse(CChar* me)
{
    CMapping* house = querymap("house");
    CMapping* r;
    char msg[255];

    if (!house) return 0;

    CVector v;

    house->keys(v);

    for (int i = 0; i < v.count(); i++) {
        const char* key = v[i];
        r               = house->querymap(key);

        char tmp[40];
        add_menu_item(snprintf(msg, 255, "%s(%s)", r->querystr("name"),
                               r->querystr("short")),
                      snprintf(tmp, 40, "$0enter 0 %s", key), me);
    }

    send_menu(me);
    return 1;
}

void setup()
{
    wait_clear = 40 + CUtils::random(40);

    load();
    CRoom::setup();
}

virtual int32_t query_attr(int attr)
{
    if (attr == CAN_SAVE) return 1;
    if (attr == IS_RESIDENT) return 1;

    return CRoom::query_attr(attr);
}

protected:
int wait_clear;

ROOM_END;

#include "CmdUser.h"
#include "../item/Money.h"
#include "../db/Sql.h"
#include "../engine/GameInit.h"
#include "../room/Room.h"

int CCmdUser::list_goods(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255], cmd[40];

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要查看谁的货物？");

    if (who == me) return notify_fail("请用红顶商人道具查看。");

    DTItemList* list = who->get_item_list();
    int i            = 0;

    for (auto val : *list) {
        CContainer* obj = val;
        int32_t val     = obj->query_temp("trade/value");

        if (!val) continue;
        if (DIFFERSTR(obj->querystr_temp("trade/owner"), who->id(1))) continue;

        snprintf(msg, 255, "%s ：%d", obj->name(), val);
        who->add_menu_item(
            msg, snprintf(cmd, 40, "$0buy $1 %ld", obj->object_id()), me);
        i++;
    }

    if (!i)
        who->say(snprintf(msg, 255, "你对%s的商品似乎不感兴趣。", who->name()),
                 me);

    who->send_menu(me);
    return 1;
}

// 购买对方的货物
int CCmdUser::buy(CChar* me, int32_t target, char* arg)
{
    CChar* who;
    char msg[255];
    int32_t val = 0;

    if (!(who = get_room_char(me, target)))
        return notify_fail("你要买看谁的货物？");

    if (who == me) return notify_fail("那还用买吗。");

    CContainer* obj = who->present(atol(arg));

    if (!obj || obj->querystr("owner")[0]
        || DIFFERSTR(obj->querystr_temp("trade/owner"), who->id(1))
        || !(val = obj->query_temp("trade/value")))
        return notify_fail("对方不打算卖给你。");

    if (!CMoney::player_pay(me, val))
        return notify_fail("你很想买%s，可是摸摸口袋，只好脸红的离开了。",
                           obj->name());

    message_vision(
        snprintf(msg, 255, "$N从$n那里买下了%s，付给$n数量%d的金币。",
                 obj->name(), val),
        me, who);

    if (!obj->move(me)) {
        obj->move(me->environment());
        tell_object(
            me, snprintf(msg, 255, "你买下了%s，但你拿不动，只好把它放在地上。",
                         obj->name()));
    }

    CMoney::pay_player(who, val);

    return 1;
}

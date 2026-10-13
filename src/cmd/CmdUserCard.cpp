#include "CmdUser.h"
#include "../item/Money.h"
#include "../db/Sql.h"
#include "../engine/GameInit.h"
#include "../room/Room.h"

int CCmdUser::card_help(CChar* me, int32_t target, char* arg)
{
    if (wizardp(me)) {
        tell_object(me, "createcard : 创建一张新月卡。");
    }
    tell_object(me, "addcard 卡号 : 注册新月卡。");
    tell_object(me, "removecard 卡号 : 移除已注册的月卡。");
    tell_object(me, "showcards : 查看自己的所有月卡。");
    return 1;
}

int CCmdUser::add_card(CChar* me, int32_t target, char* arg)
{
    if (!register_card(arg, me->id(1))) {
        return false;
    }

    tell_object(me, "注册成功！");
    const char* name = (me->environment())->querystr("base_name");
    if (EQUALSTR(name, "rk")) {
        me->move(load_room(g_start_room));
    }

    return 1;
}

int CCmdUser::remove_card(CChar* me, int32_t target, char* arg)
{
    if (!unregister_card(arg, me->id(1))) {
        return false;
    }

    tell_object(me, "移除成功！");

    char endtime[64] = "0000/00/00 00:00:00";
    if (!verify_id(me->id(1), endtime)) {
        char msg[256];
        tell_object(
            me, snprintf(msg, 255, "$HIY您的账户已于 %s 到期，请联系巫师续费。",
                         endtime));
        me->move(load_room("rk"));
    }
    return 1;
}

int CCmdUser::show_cards(CChar* me, int32_t target, char* arg)
{
    ::show_cards(me);
    return 1;
}

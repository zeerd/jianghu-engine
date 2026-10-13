/**
 * @file BankOwner.cpp
 * @brief implementation of the CBankOwner class. */

#include <windows.h>

#include "BankOwner.h"
#include "Money.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------

CBankOwner::CBankOwner() {}

CBankOwner::~CBankOwner() {}

// 存钱 deposit
int CBankOwner::do_deposit(CChar* me, int32_t amount)
{
    char msg[255];

    if (amount <= 0) return 0;

    if (!CMoney::player_pay(me, amount))
        return notify_fail("你身上没有那麽多钱。");

    if (me->query("balance") > LONGMAX - amount) {
        return notify_fail("我们没办法帮你保存更多的钱了。");
    }

    me->add_balance(amount);
    me->save();
    message_vision(
        snprintf(msg, 255, "$N存入现款%s。", CMoney::money_str(amount)), me);

    LOG_INFO("%s(%s)存入现款%ld，存款达到%ld。", me->name(1), me->id(1), amount,
             me->query("balance"));

    return 1;
}

// 提款
int CBankOwner::do_withdraw(CChar* me, int32_t amount)
{
    char msg[255];
    int32_t hamount;

    hamount = me->query("balance");

    if (amount <= 0) return 0;
    if (amount > hamount) return notify_fail("你没这么多存款 ！！");

    me->add_balance(-amount);
    me->save();

    CMoney::pay_player(me, amount);
    tell_object(me, "提款完毕。");

    snprintf(msg, 255, "%s(%s)提款%d，余额%d。From (%s)", me->name(1),
             me->id(1), amount, me->query("balance"), get_ip(me));
    LOG_INFO("%s", msg);

    return 1;
}

// 贷款
int CBankOwner::do_loan(CChar* me, int32_t amount)
{
    int32_t hamount;
    char msg[255];

    hamount = me->query("balance");
    if (hamount > 0) return notify_fail("你还有存款！");

    if (me->query("level") < 80)
        return notify_fail("你的江湖地位不够哦，再历练一下吧！");

    if (me->query("level") > 170)
        return notify_fail("这么有名的人物还来借钱？肯定是假冒的，不借！");

    if (amount <= 0) return 0;
    if (amount > 0 && hamount < 0 && amount > LONGMAX - abs(hamount))
        return notify_fail("$HIR别想刷钱！");
    if (CChar::level2_exp(me->query("level")) < amount - hamount)
        return notify_fail("你借不到这么多钱．．．");

    hamount -= amount;
    me->set("balance", hamount);
    me->save();

    CMoney::pay_player(me, amount);
    tell_object(me, "贷款完毕。");

    snprintf(msg, 255, "%s(%s)贷款%d，余额%d。From (%s)", me->name(1),
             me->id(1), amount, me->query("balance"), get_ip(me));
    LOG_INFO("%s", msg);

    return 1;
}

int CBankOwner::handle_action(char* comm, CChar* me, char* arg)
{
    if (is_fighting()) return 0;

    if (strcmp(comm, "deposit") == 0) return do_deposit(me, atol(arg));
    if (strcmp(comm, "withdraw") == 0) return do_withdraw(me, atol(arg));
    if (strcmp(comm, "loan") == 0) return do_loan(me, atol(arg));
    if (strcmp(comm, "balance") == 0) return do_balance(me);

    return 0;
}

int32_t CBankOwner::query_attr(int attr)
{
    if (attr == IS_BANKER) return 1;

    return CNpc::query_attr(attr);
}

int CBankOwner::do_balance(CChar* me)
{
    char msg[255];
    int32_t value = me->query("balance");

    if (value == 0) {
        message_vision_room(me, "tell", "你现在没有存款。");
        return 1;
    }

    if (value > 0)
        snprintf(msg, 255, "你现在共有存款：%s。", CMoney::money_str(value));
    else
        snprintf(msg, 255, "你现在共有欠款：%s。", CMoney::money_str(-value));

    message_vision_room(me, "tell", msg);
    return 1;
}

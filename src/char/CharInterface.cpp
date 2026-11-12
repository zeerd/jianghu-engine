/**
 * @file CharInterface.cpp
 * @brief CChar 实现的接口方法（IHittable / IEquippable
 * / IActionHandler / IDialogSource / IClientObject）。 */

#include <windows.h>
#include <cstring>
#include <cstdio>

#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Room.h"
#include "Sql.h"

// ========================================================================
//  IHittable
// ========================================================================

const char* CChar::hit_ob(CChar* me, CChar* victim, damage_t damage_bonus)
{
    return "";
}

void CChar::owner_is_killed(CChar* killer) {}

// ========================================================================
//  IEquippable
// ========================================================================

// 角色侧的装备接口占位：装备动作的语义主体是物品——wield 命令与
// 脚本 :wield() 都对物品做 do_action(DO_WIELD)，由 CEquip::do_wield
// 完成装备；角色侧恒返回 0，wield()/wear() 对角色无效果。
// 不可删的理由是接口契约而非功能：二者实现 IEquippable 纯虚函数，
// 只删定义会链接失败；连声明一起删则 CChar 缺实现成为抽象类，
// CNpc/CUser 等实例化点全部编译失败；彻底删除须连 Char.h 的
// public IEquippable 一并摘除，那会翻转 l_wield/l_wear 对角色的
// 返回契约（转换成功返回 0、失败返回 nil，0 为真 nil 为假），
// 角色也不再属于「可装备对象」类型面（CChar 与 CItem 同实现该接口）。
int CChar::wield() { return do_action(DO_WIELD); }

// 同 wield：角色侧没有独立的穿戴动作，与 wield 共用 DO_WIELD 语义。
int CChar::wear() { return do_action(DO_WIELD); }

// ========================================================================
//  IActionHandler
// ========================================================================

// 角色自身不处理装备/使用类动作，默认恒返回 0；需要响应的动作由
// 具体实现类（CEquip、CVendor、CRoom）覆写。同 IEquippable：
// do_action 是 IActionHandler 的纯虚实现，同样不可单独删除。
int CChar::do_action(int act, CChar* me) { return 0; }

int CChar::handle_action(char* comm, CChar* me, char* arg) { return 0; }

int CChar::handle_action(char* comm, CChar* me, char* arg, char* target)
{
    for (auto obj : m_item_list) {
        if (obj == me) continue;
        IActionHandler* handler = dynamic_cast<IActionHandler*>(obj);
        if (handler && handler->handle_action(comm, me, arg, target)) return 1;
    }
    return 0;
}

// ========================================================================
//  IDialogSource
// ========================================================================

void CChar::send_menu(CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&e=1&", QUESTION, object_id());
    me->send_command(msg, SEND_WAIT_BUFFERED);

    m_begin_menu = 0;
}

void CChar::add_menu_item(string show, string cmd, CChar* me)
{
    char msg[512];

    snprintf(msg, 512, "&C=%d&N=%d&s=%s&c=%s&", QUESTION, object_id(),
             show.c_str(), cmd.c_str());
    if (m_begin_menu) {
        strcat(msg, "b=1&");
        m_begin_menu = 0;
    }

    me->send_command(msg, SEND_WAIT_BUFFERED);
}

int CChar::say(const char* arg, CChar* me, const char* tips)
{
    char chat[402];
    char msg[512];

    if (!me) {
        strncpy(chat, arg, 400)[400] = 0;
        snprintf(msg, 512, "%s说：%s", name(), chat);
        message_vision_room(this, "chat", msg);
    }
    else {
        me->send_command(snprintf(msg, 512, "&C=%d&N=%d&n=%s&m=%s&", QUESTION,
                                  object_id(), tips ? tips : name(), arg),
                         SEND_WAIT_BUFFERED);
    }

    return 1;
}

// ========================================================================
//  IClientObject
// ========================================================================

void CChar::download(CChar* me)
{
    char str[512];

    snprintf(str, 512, "&C=%d&O=%d&", DOWNOBJECT, object_id());
    get_info(str, me);

    me->send_command(str, SEND_WAIT_BUFFERED);
}

void CChar::get_info(char* str, CChar* me)
{
    char tmp[512], msg[255];

    snprintf(tmp, 255, "N=%s&I=%s&P=%lld&a=%lld&U=%d&W=%lld&", name(), id(),
             (long long)query("icon"), (long long)query_amount(),
             query_attr(CChar::UPDATE), (long long)weight());

    // NPC角色传活动范围坐标
    if (query_attr(IS_CHAR) && !query_attr(IS_USER)) {
        snprintf(msg, 255, "bx=%d&by=%d&ex=%d&ey=%d&dr=%d&ac=%d&",
                 query("map/bx"), query("map/by"), query("map/ex"),
                 query("map/ey"), query("map/dir"), query("map/active"));
        strcat(tmp, msg);
    }

    if (query_attr(CAN_GET)) strcat(tmp, "g=1&");
    if (query_attr(IS_BOX)) strcat(tmp, "x=1&");

    if (EQUALSTR(querystr("status"), "locked")) {
        strcat(tmp, snprintf(msg, 40, "l=%s&", querystr("reason")));
    }

    if (query_attr(IS_VENDOR)) strcat(tmp, "v=1&");
    if (query_attr(IS_BANKER)) strcat(tmp, "b=1&");

    if (querymap("skill")) strcat(tmp, "bk=1&");  // 可读的书

    if (query_attr(IS_CHAR)
        && (me->is_apprentice_of(this) || recognize_apprentice(me)))
        strcat(tmp, "m=1&");
    if (query_attr(CAN_USE)) strcat(tmp, "f=1&");

    if (query_attr(CAN_EQUIPPED)) {
        snprintf(msg, 40, "w=%d&", query("wield_position"));
        strcat(tmp, msg);
    }

    strcat(tmp, snprintf(msg, 40, "t=%s&", title()));

    strcat(str, tmp);
}

void CChar::send_command(const char* msg, int wait) {}

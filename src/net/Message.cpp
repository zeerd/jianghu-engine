/**
 * @file Message.cpp
 * @brief 信息处理 */

#include <windows.h>

#include <vector>

#include "Global.h"
#include "User.h"
#include "Channel.h"
#include "Room.h"
#include "IClientObject.h"

extern MAP2USER g_user_list;  // User List

char* chinese_number(mapping_value_t i, char* msg)
{
    static char c_digit[][3] = {"零", "十", "百", "千", "万", "亿", "兆"};
    static char c_num[][3]
        = {"零", "一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};
    char tmp[255], nex[255];

    if (i < 0) {
        return snprintf(msg, 255, "负%s", chinese_number(-i, tmp));
    }

    if (i < 11) {
        strcpy(msg, c_num[i]);
    }
    else if (i < 20) {
        snprintf(msg, 255, "%s%s", c_num[10], c_num[i - 10]);
    }
    else if (i < 100) {
        if (i % 10)
            snprintf(msg, 255, "%s%s%s", c_num[i / 10], c_digit[1],
                     c_num[i % 10]);
        else
            snprintf(msg, 255, "%s%s", c_num[i / 10], c_digit[1]);
    }
    else if (i < 1000) {
        if (i % 100 == 0)
            snprintf(msg, 255, "%s%s", c_num[i / 100], c_digit[2]);
        else if (i % 100 < 10)
            snprintf(msg, 255, "%s%s%s%s", c_num[i / 100], c_digit[2], c_num[0],
                     chinese_number(i % 100, nex));
        else if (i % 100 < 20)
            snprintf(msg, 255, "%s%s%s%s", c_num[i / 100], c_digit[2], c_num[1],
                     chinese_number(i % 100, nex));
        else
            snprintf(msg, 255, "%s%s%s", c_num[i / 100], c_digit[2],
                     chinese_number(i % 100, nex));
    }
    else if (i < 10000) {
        if (i % 1000 == 0)
            snprintf(msg, 255, "%s%s", c_num[i / 1000], c_digit[3]);
        else if (i % 1000 < 100)
            snprintf(msg, 255, "%s%s%s%s", c_num[i / 1000], c_digit[3],
                     c_digit[0], chinese_number(i % 1000, nex));
        else
            snprintf(msg, 255, "%s%s%s", c_num[i / 1000], c_digit[3],
                     chinese_number(i % 1000, nex));
    }
    else if (i < 100000000L) {
        if (i % 10000 == 0)
            snprintf(msg, 255, "%s%s", chinese_number(i / 10000, nex),
                     c_digit[4]);
        else if (i % 10000 < 1000)
            snprintf(msg, 255, "%s%s%s%s", chinese_number(i / 10000, tmp),
                     c_digit[4], c_digit[0], chinese_number(i % 10000, nex));
        else
            snprintf(msg, 255, "%s%s%s", chinese_number(i / 10000, tmp),
                     c_digit[4], chinese_number(i % 10000, nex));
    }
    else {
        if (i % 100000000L == 0)
            snprintf(msg, 255, "%s%s", chinese_number(i / 100000000L, tmp),
                     c_digit[5]);
        else if (i % 100000000L < 10000000L)
            snprintf(msg, 255, "%s%s%s%s", chinese_number(i / 100000000L, tmp),
                     c_digit[5], c_digit[0],
                     chinese_number(i % 100000000L, nex));
        else
            snprintf(msg, 255, "%s%s%s", chinese_number(i / 100000000L, tmp),
                     c_digit[5], chinese_number(i % 100000000L, nex));
    }

    return msg;
}

const char* gender_self(string sex) { return "你"; }

const char* gender_pronoun(string sex)
{
    if (sex == "男性")
        return "他";
    else if (sex == "女性")
        return "她";
    else
        return "它";
}

void tell_object(CContainer* ob, const char* str, const char* channel)
{
    if (is_user(ob)) {
        if (channel && (static_cast<CUser*>(ob)->refuse_channel(channel)))
            return;
        message_vision_room(ob, "tell", str);
    }
    else if (ob) {
        MAP2USER list;
        ob->get_users(channel, &list);
        message(channel, str, &list);
    }
}

void tell_room(CContainer* ob, const char* str, CChar* exclude,
               const char* channel)
{
    if (ob) {
        MAP2USER list;
        ob->get_users(channel, &list);
        message(channel, str, &list, exclude);
    }
}

// 带频道的输出
void message_channel(const char* channel, const char* msg, CChar* me,
                     CContainer* you)
{
    if (!me) return;

    const std::string my_name   = truncated_copy(me->name(), 39);
    const std::string my_gender = truncated_copy(me->querystr("gender"), 9);

    if (my_name.empty()) return;  // mon 10/23/97

    // 防止颜色混淆$NOR->$COM
    std::string str1 = truncated_copy(msg, 511);
    replace_string(str1, "$NOR", "$COM", 512);

    std::string str2 = str1;
    std::string str3 = str1;

    replace_string(str1, "$P", gender_self(my_gender), 512);
    replace_string(str1, "$N", gender_self(my_gender), 512);

    replace_string(str3, "$P", my_name.c_str(), 512);
    replace_string(str3, "$N", my_name.c_str(), 512);

    if (you) {
        const std::string your_name = truncated_copy(you->name(), 39);
        const std::string your_gender
            = truncated_copy(you->querystr("gender"), 9);

        replace_string(str2, "$P", gender_pronoun(my_gender), 512);
        replace_string(str2, "$p", gender_self(your_gender), 512);
        replace_string(str2, "$N", my_name.c_str(), 512);
        replace_string(str2, "$n", gender_self(your_gender), 512);

        if (is_user(you)
            && !(static_cast<CChar*>(you)->refuse_channel(channel)))
            tell_object(you, str2.c_str());

        replace_string(str1, "$p", gender_pronoun(your_gender), 512);
        replace_string(str1, "$n", your_name.c_str(), 512);
        replace_string(str3, "$p", your_name.c_str(), 512);
        replace_string(str3, "$n", your_name.c_str(), 512);
    }

    if (!me->refuse_channel(channel)) tell_object(me, str1.c_str());

    CContainer* room = me->environment();
    if (room) {
        MAP2USER list;
        room->get_users(channel, &list);
        message(channel, str3.c_str(), &list, me, you);
    }
}

void message_vision(const char* msg, CContainer* me, CContainer* you, bool self)
{
    if (!me) return;

    const std::string my_name   = truncated_copy(me->name(), 39);
    const std::string my_gender = truncated_copy(me->querystr("gender"), 9);

    if (my_name.empty()) return;  // mon 10/23/97

    // 防止颜色混淆$NOR->$COM
    std::string str1 = truncated_copy(msg, 511);
    replace_string(str1, "$NOR", "$COM", 512);

    std::string str2 = str1;
    std::string str3 = str1;

    replace_string(str1, "$P", gender_self(my_gender), 512);
    replace_string(str1, "$N", gender_self(my_gender), 512);

    replace_string(str3, "$P", my_name.c_str(), 512);
    replace_string(str3, "$N", my_name.c_str(), 512);

    if (you) {
        const std::string your_name = truncated_copy(you->name(), 39);
        const std::string your_gender
            = truncated_copy(you->querystr("gender"), 9);

        replace_string(str2, "$P", gender_pronoun(my_gender), 512);
        replace_string(str2, "$p", gender_self(your_gender), 512);
        replace_string(str2, "$N", my_name.c_str(), 512);
        replace_string(str2, "$n", gender_self(your_gender), 512);
        message_vision_room(you, "tell", str2.c_str());

        replace_string(str1, "$p", gender_pronoun(your_gender), 512);
        replace_string(str1, "$n", your_name.c_str(), 512);
        replace_string(str3, "$p", your_name.c_str(), 512);
        replace_string(str3, "$n", your_name.c_str(), 512);
    }

    message_vision_room(me, "tell", str1.c_str());

    if (me->environment() && !self)
        message_vision_room(me, "chat", str3.c_str(), me, you);
}

// 向频道内所有玩家发送消息
// 这个函数很费时
void message(const char* channel, const char* chat, MAP2USER* ulist,
             CContainer* me, CContainer* who)
{
    char msg[512];
    iterator_user pos;

    snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);

    for (pos = ulist->begin(); pos != ulist->end(); pos++) {
        CUser* obj = pos->second;
        if (obj == me || obj == who) continue;
        dynamic_cast<IClientObject*>(obj)->send_command(msg,
                                                        SEND_WAIT_IMMEDIATE);
    }
}

void message_vision_room(CContainer* me, const char* room, const char* chatmsg,
                         CContainer* exclude, CContainer* exclude2)
{
    DTItemList* t;
    iterator_user pos;
    CContainer *obj, *env;
    char msg[512];

    env = me->environment();

    const std::string chat = truncated_copy(chatmsg, 479);

    if (strcmp(room, "notify") == 0)  // 通知消息，范围是全部玩家
    {
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat.c_str());
        for (pos = g_user_list.begin(); pos != g_user_list.end(); pos++)
            (pos->second)->send_command(msg, SEND_WAIT_IMMEDIATE);

        return;
    }

    if (strcmp(room, "send_all") == 0)  // 通知命令，范围是全部玩家
    {
        for (pos = g_user_list.begin(); pos != g_user_list.end(); pos++)
            (pos->second)->send_command(chat.c_str(), SEND_WAIT_IMMEDIATE);

        return;
    }

    if (strcmp(room, "chat") == 0)  // 环境内玩家
    {
        if (!env) return;
        t = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat.c_str());
        for (auto val : *t) {
            obj = val;
            if (obj == exclude || obj == exclude2) continue;
            if (obj->query_attr(CChar::IS_USER))  // 是玩家
                dynamic_cast<IClientObject*>(obj)->send_command(
                    msg, SEND_WAIT_IMMEDIATE);
        }

        // 信息发送给环境
        const char* ch = env->querystr("channel_cast");  // 房间内的频道转播
        if (ch[0]) {
            g_channel.do_channel(env, NULL, ch, chat.c_str());
        }

        return;
    }

    if (strcmp(room, "room") == 0)  // 环境内玩家
    {
        env = me;
        t   = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat.c_str());
        for (auto val : *t) {
            obj = val;
            if (obj == exclude) continue;
            if (obj->query_attr(CChar::IS_USER))  // 是玩家
                dynamic_cast<IClientObject*>(obj)->send_command(
                    msg, SEND_WAIT_IMMEDIATE);
        }

        // 信息发送给环境
        const char* ch = env->querystr("channel_cast");  // 房间内的频道转播
        if (ch[0]) {
            g_channel.do_channel(env, NULL, ch, chat.c_str());
        }

        return;
    }

    // 其他频道
    if (strcmp(room, "sys") == 0)  // 系统频道，只有巫师能看得到。
    {
        if (!env) return;
        t = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat.c_str());
        for (auto val : *t) {
            obj = val;

            if (is_wizard(static_cast<CChar*>(obj)))  // 是巫师
                dynamic_cast<IClientObject*>(obj)->send_command(
                    msg, SEND_WAIT_IMMEDIATE);
        }
        return;
    }

    // 通知频道
    if (strcmp(room, "tell") == 0) {
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat.c_str());
        dynamic_cast<IClientObject*>(me)->send_command(msg,
                                                       SEND_WAIT_IMMEDIATE);
    }

    // 发布命令通道
    if (strcmp(room, "command") == 0) {
        if (!env) return;
        const std::string cmd = truncated_copy(chatmsg, 511);
        t                     = env->get_item_list();
        for (auto val : *t) {
            obj = val;
            if (obj == exclude) continue;
            if (auto* cli = dynamic_cast<IClientObject*>(obj);
                cli && cli->has_client())  // 有客户端才投得出去
                cli->send_command(cmd.c_str(), SEND_WAIT_IMMEDIATE);
        }
        return;
    }

    // 执行指令通道
    if (strcmp(room, "execute") == 0) {
        if (!env) return;
        strncpy(msg, chatmsg, 512)[511] = 0;
        t                               = env->get_item_list();
        for (auto val : *t) {
            obj = val;
            if (obj->query_attr(CChar::IS_USER))  // 是玩家
                static_cast<CChar*>(obj)->command(msg);
        }
        return;
    }
}

// 用newstr替换msg中的oldstr：截到 len-1（宽度契约），替换产物不回扫
void replace_string(std::string& msg, const char* oldstr, const char* newstr,
                    size_t len)
{
    if (len == 0) {
        msg.clear();
        return;
    }

    std::string out;
    out.reserve(msg.size());
    size_t p = 0;
    while (p < msg.size()) {
        size_t t    = 0;
        size_t mark = out.size();
        while (p < msg.size() && msg[p] == oldstr[t]) {
            if (out.size() == len - 1) break;
            out.push_back(msg[p]);
            p++;
            t++;
            if (!oldstr[t])  // 完整命中：回卷到匹配起点，改写为 newstr
            {
                out.resize(mark);
                for (size_t n = 0; newstr[n] && out.size() < len - 1; n++)
                    out.push_back(newstr[n]);
            }
        }
        if (out.size() == len - 1) break;
        if (p >= msg.size()) break;  // 终止符不入结果
        out.push_back(msg[p]);
        p++;
    }
    msg = std::move(out);
}

void replace_string(char* msg, const char* oldstr, const char* newstr,
                    int nMsgLen)
{
    if (nMsgLen <= 0) return;
    std::string out(msg);
    replace_string(out, oldstr, newstr, static_cast<size_t>(nMsgLen));
    strcpy(msg, out.c_str());
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// tell_object(ob, msg[, channel])
int l_tell_object(lua_State* L)
{
    CMainObject* obj    = get_object(L, 1);
    const char* msg     = luaL_checkstring(L, 2);
    const char* channel = "system";
    if (lua_isstring(L, 3)) channel = lua_tostring(L, 3);
    if (obj) tell_object(dynamic_cast<CContainer*>(obj), msg, channel);
    return 0;
}

// message(kind, ...) -> nil - 消息分派口。KIND 取 vision（房间视野）/
// vision_self（私发双方）/ vision_room（按频道的房间视野）/ channel（按频道
// 投递）/ room（指定房间裸文本）；后续参数与原各消息函数一致，整体右移一位。
// 未登记 KIND 报错。
int l_message(lua_State* L)
{
    const char* kind = luaL_checkstring(L, 1);
    if (strcmp(kind, "vision") != 0 && strcmp(kind, "vision_self") != 0
        && strcmp(kind, "vision_room") != 0 && strcmp(kind, "channel") != 0
        && strcmp(kind, "room") != 0)
        return luaL_error(L, "unknown message kind: %s", kind);

    if (strcmp(kind, "vision") == 0 || strcmp(kind, "vision_self") == 0) {
        const char* msg = luaL_checkstring(L, 2);
        CContainer* me  = NULL;
        CContainer* you = NULL;
        if (lua_isuserdata(L, 3))
            me = dynamic_cast<CContainer*>(get_object(L, 3));
        if (lua_isuserdata(L, 4))
            you = dynamic_cast<CContainer*>(get_object(L, 4));
        message_vision(msg, me, you, strcmp(kind, "vision_self") == 0);
        return 0;
    }

    if (strcmp(kind, "vision_room") == 0) {
        CContainer* me = NULL;
        if (lua_isuserdata(L, 2))
            me = dynamic_cast<CContainer*>(get_object(L, 2));
        const char* channel = luaL_checkstring(L, 3);
        const char* msg     = luaL_checkstring(L, 4);
        CContainer* e1      = NULL;
        CContainer* e2      = NULL;
        if (lua_isuserdata(L, 5))
            e1 = dynamic_cast<CContainer*>(get_object(L, 5));
        if (lua_isuserdata(L, 6))
            e2 = dynamic_cast<CContainer*>(get_object(L, 6));
        message_vision_room(me, channel, msg, e1, e2);
        return 0;
    }

    if (strcmp(kind, "channel") == 0) {
        const char* channel = luaL_checkstring(L, 2);
        const char* msg     = luaL_checkstring(L, 3);
        CChar* me           = NULL;
        if (lua_isuserdata(L, 4)) me = dynamic_cast<CChar*>(get_object(L, 4));
        if (!me) return 0;
        CContainer* you = NULL;
        if (lua_isuserdata(L, 5))
            you = dynamic_cast<CContainer*>(get_object(L, 5));
        message_channel(channel, msg, me, you);
        return 0;
    }

    // kind == "room"：指定房间的裸文本广播（原 tell_room）
    CContainer* room = NULL;
    if (lua_isuserdata(L, 2))
        room = dynamic_cast<CContainer*>(get_object(L, 2));
    const char* msg = luaL_checkstring(L, 3);
    CChar* exclude  = NULL;
    if (lua_isuserdata(L, 4)) exclude = dynamic_cast<CChar*>(get_object(L, 4));
    if (room) tell_room(room, msg, exclude);
    return 0;
}

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

char* chinese_number(signed long int i, char* msg)
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
    if (userp(ob)) {
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
    char my_gender[10], your_gender[10], my_name[40], your_name[40];
    char str1[512], str2[512], str3[512];

    if (!me) return;

    strncpy(my_name, me->name(), 40);
    strncpy(my_gender, me->querystr("gender"), 10);

    if (!my_name[0]) return;  // mon 10/23/97

    // 防止颜色混淆$NOR->$COM
    strncpy(str1, msg, 512)[511] = 0;
    replace_string(str1, "$NOR", "$COM", 512);

    memcpy(str2, str1, 512);
    memcpy(str3, str1, 512);

    replace_string(str1, "$P", gender_self(my_gender), 512);
    replace_string(str1, "$N", gender_self(my_gender), 512);

    replace_string(str3, "$P", my_name, 512);
    replace_string(str3, "$N", my_name, 512);

    if (you) {
        strncpy(your_name, you->name(), 40);
        strncpy(your_gender, you->querystr("gender"), 10);

        replace_string(str2, "$P", gender_pronoun(my_gender), 512);
        replace_string(str2, "$p", gender_self(your_gender), 512);
        replace_string(str2, "$N", my_name, 512);
        replace_string(str2, "$n", gender_self(your_gender), 512);

        if (userp(you) && !(static_cast<CChar*>(you)->refuse_channel(channel)))
            tell_object(you, str2);

        replace_string(str1, "$p", gender_pronoun(your_gender), 512);
        replace_string(str1, "$n", your_name, 512);
        replace_string(str3, "$p", your_name, 512);
        replace_string(str3, "$n", your_name, 512);
    }

    if (!me->refuse_channel(channel)) tell_object(me, str1);

    CContainer* room = me->environment();
    if (room) {
        MAP2USER list;
        room->get_users(channel, &list);
        message(channel, str3, &list, me, you);
    }
}

void message_vision(const char* msg, CContainer* me, CContainer* you, bool self)
{
    char my_gender[10], your_gender[10], my_name[40], your_name[40];
    char str1[512], str2[512], str3[512];

    if (!me) return;

    strncpy(my_name, me->name(), 40);
    strncpy(my_gender, me->querystr("gender"), 10);

    if (!my_name[0]) return;  // mon 10/23/97

    // 防止颜色混淆$NOR->$COM
    strncpy(str1, msg, 512)[511] = 0;
    replace_string(str1, "$NOR", "$COM", 512);

    memcpy(str2, str1, 512);
    memcpy(str3, str1, 512);

    replace_string(str1, "$P", gender_self(my_gender), 512);
    replace_string(str1, "$N", gender_self(my_gender), 512);

    replace_string(str3, "$P", my_name, 512);
    replace_string(str3, "$N", my_name, 512);

    if (you) {
        strncpy(your_name, you->name(), 40);
        strncpy(your_gender, you->querystr("gender"), 10);

        replace_string(str2, "$P", gender_pronoun(my_gender), 512);
        replace_string(str2, "$p", gender_self(your_gender), 512);
        replace_string(str2, "$N", my_name, 512);
        replace_string(str2, "$n", gender_self(your_gender), 512);
        message_vision_room(you, "tell", str2);

        replace_string(str1, "$p", gender_pronoun(your_gender), 512);
        replace_string(str1, "$n", your_name, 512);
        replace_string(str3, "$p", your_name, 512);
        replace_string(str3, "$n", your_name, 512);
    }

    message_vision_room(me, "tell", str1);

    if (me->environment() && !self)
        message_vision_room(me, "chat", str3, me, you);
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
    char chat[480];
    char msg[512];

    env = me->environment();

    strncpy(chat, chatmsg, 480)[479] = 0;  // 防止字符越界

    if (strcmp(room, "notify") == 0)  // 通知消息，范围是全部玩家
    {
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);
        for (pos = g_user_list.begin(); pos != g_user_list.end(); pos++)
            (pos->second)->send_command(msg, SEND_WAIT_IMMEDIATE);

        return;
    }

    if (strcmp(room, "send_all") == 0)  // 通知命令，范围是全部玩家
    {
        for (pos = g_user_list.begin(); pos != g_user_list.end(); pos++)
            (pos->second)->send_command(chat, SEND_WAIT_IMMEDIATE);

        return;
    }

    if (strcmp(room, "chat") == 0)  // 环境内玩家
    {
        if (!env) return;
        t = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);
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
            g_channel.do_channel(env, NULL, ch, chat);
        }

        return;
    }

    if (strcmp(room, "room") == 0)  // 环境内玩家
    {
        env = me;
        t   = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);
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
            g_channel.do_channel(env, NULL, ch, chat);
        }

        return;
    }

    // 其他频道
    if (strcmp(room, "sys") == 0)  // 系统频道，只有巫师能看得到。
    {
        if (!env) return;
        t = env->get_item_list();
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);
        for (auto val : *t) {
            obj = val;

            if (wizardp(static_cast<CChar*>(obj)))  // 是巫师
                dynamic_cast<IClientObject*>(obj)->send_command(
                    msg, SEND_WAIT_IMMEDIATE);
        }
        return;
    }

    // 通知频道
    if (strcmp(room, "tell") == 0) {
        snprintf(msg, 512, "&C=%d&M=%s&", NOTIFY, chat);
        dynamic_cast<IClientObject*>(me)->send_command(msg,
                                                       SEND_WAIT_IMMEDIATE);
    }

    // 发布命令通道
    if (strcmp(room, "command") == 0) {
        if (!env) return;
        strncpy(msg, chatmsg, 512)[511] = 0;
        t                               = env->get_item_list();
        for (auto val : *t) {
            obj = val;
            if (obj == exclude) continue;
            if (auto* cli = dynamic_cast<IClientObject*>(obj);
                cli && cli->has_client())  // 有客户端才投得出去
                cli->send_command(msg, SEND_WAIT_IMMEDIATE);
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

// 用newstr替换msg中的oldstr
void replace_string(char* msg, const char* oldstr, const char* newstr,
                    int nMsgLen)
{
    int p = 0, t = 0, r = 0, mark;

    std::vector<char> replace(nMsgLen);  // 最大长度

    while (msg[p]) {
        t    = 0;
        mark = r;
        while (msg[p] && msg[p] == oldstr[t]) {
            if (r == nMsgLen - 1) break;
            replace[r++] = msg[p];
            p++;
            t++;
            if (!oldstr[t])  // 找到替换字符串了。
            {
                int n = 0;
                r     = mark;
                while (newstr[n]) {
                    if (r == nMsgLen - 1) break;
                    replace[r++] = newstr[n++];
                }
            }
        }

        if (r == nMsgLen - 1) break;
        replace[r++] = msg[p++];
    }
    replace[r] = 0;
    strcpy(msg, replace.data());
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

// message_vision(msg, me, you)
int l_message_vision(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    CContainer* me  = NULL;
    CContainer* you = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CContainer*>(get_object(L, 2));
    if (lua_isuserdata(L, 3)) you = dynamic_cast<CContainer*>(get_object(L, 3));
    message_vision(msg, me, you);
    return 0;
}

// message_vision_self(msg, me, you) - 只私发双方、不向房间广播
// （对应 C++ message_vision 的 self=true；既有 message_vision 的第 4 参
//  历史上被忽略，不能挪用，故独立成键）
int l_message_vision_self(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    CContainer* me  = NULL;
    CContainer* you = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CContainer*>(get_object(L, 2));
    if (lua_isuserdata(L, 3)) you = dynamic_cast<CContainer*>(get_object(L, 3));
    message_vision(msg, me, you, true);
    return 0;
}

// MessageVision(me, channel, msg[, exclude1[, exclude2]])
int l_message_vision_channel(lua_State* L)
{
    CContainer* me = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CContainer*>(get_object(L, 1));
    const char* channel = luaL_checkstring(L, 2);
    const char* msg     = luaL_checkstring(L, 3);
    CContainer* e1      = NULL;
    CContainer* e2      = NULL;
    if (lua_isuserdata(L, 4)) e1 = dynamic_cast<CContainer*>(get_object(L, 4));
    if (lua_isuserdata(L, 5)) e2 = dynamic_cast<CContainer*>(get_object(L, 5));
    message_vision_room(me, channel, msg, e1, e2);
    return 0;
}

// message_channel(channel, msg, me[, you])  （全局 message_channel）
// 按频道向 me 所在房间内订阅该频道的用户广播（含 refuse_channel 检查）
int l_message_channel(lua_State* L)
{
    const char* channel = luaL_checkstring(L, 1);
    const char* msg     = luaL_checkstring(L, 2);
    CChar* me           = NULL;
    if (lua_isuserdata(L, 3)) me = dynamic_cast<CChar*>(get_object(L, 3));
    if (!me) return 0;
    CContainer* you = NULL;
    if (lua_isuserdata(L, 4)) you = dynamic_cast<CContainer*>(get_object(L, 4));
    message_channel(channel, msg, me, you);
    return 0;
}

// chinese_number(i) -> string  （Global.h chinese_number：数字转中文数字）
int l_chinese_number(lua_State* L)
{
    signed long int i = (signed long int)luaL_checkinteger(L, 1);
    static char buf[255];
    const char* s = chinese_number(i, buf);
    lua_pushstring(L, s ? s : "");
    return 1;
}

// tell_room(room, msg, exclude)
int l_tell_room(lua_State* L)
{
    CContainer* room = NULL;
    if (lua_isuserdata(L, 1))
        room = dynamic_cast<CContainer*>(get_object(L, 1));
    const char* msg = luaL_checkstring(L, 2);
    CChar* exclude  = NULL;
    if (lua_isuserdata(L, 3)) exclude = dynamic_cast<CChar*>(get_object(L, 3));
    if (room) tell_room(room, msg, exclude);
    return 0;
}

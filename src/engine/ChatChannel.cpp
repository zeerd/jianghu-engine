/** @file ChatChannel.cpp @brief ChatChannel implementation. */

#include "ChatChannel.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Char.h"
#include "EmoteSystem.h"
#include "Global.h"
#include "LuaEngine.h"
#include "LuaHook.h"
#include "Message.h"
#include "Npc.h"
#include "Room.h"
#include "User.h"

//---------------------------------------------------------
// Helper: string search
//---------------------------------------------------------
#define IS_ENGLISH_OR_NUMBER(last)                                \
    ((last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z') \
     || (last >= '0' && last <= '9'))

// 在字符串里搜索匹配字符串
static int lstrchr(const unsigned char* src, const unsigned char* part)
{
    int s     = 0;
    int dchar = 0;
    unsigned char ch, last = 0;

    ch = src[s];
    while (ch) {
        int cmp = 1;
        if (ch > 0xa0) {
            if (dchar) cmp = 0;
            dchar = 1 - dchar;
        }
        else {
            dchar = 0;

            // 如果前面为英文,如连起来的单词.不比较,除非是汉字或空格.
            if (IS_ENGLISH_OR_NUMBER(last)) cmp = 0;
        }

        if (cmp)  // 比较起始点
        {
            int c = s, p = 0;

            while (src[c] == part[p] || (part[p] > 0xb0 && src[c] < 0xb0)) {
                if (src[c] == part[p]) p++;
                c++;

                if (!part[p]) {
                    // 判断后面的是否还为英文,如单词中间部分,则不算。
                    if (!IS_ENGLISH_OR_NUMBER(src[c])) return s + 1;
                }
                if (!src[c]) return 0;
            }
        }

        last = ch;
        ch   = src[++s];
    }

    return 0;
}

//---------------------------------------------------------
// create — 初始化频道配置表
//---------------------------------------------------------
void CChatChannel::create()
{
    clear();

    // 1，建立频道
    CMapping m;

    // sys
    m.set("msg_speak", "$BLU【系统】%s：%s");
    m.set("msg_color", "$BLU");
    m.set("wiz_only", 1);
    m.set("channel", "sys");

    channels.set("sys", m);
    m.clear();

    // new
    m.set("msg_speak", "$HIG【%s】%s：%s");
    m.set("msg_emote", "$HIG【新手】%s");
    m.set("msg_color", "$HIG");
    m.set("newbie", 1);
    m.set("channel", "new");

    channels.set("new", m);
    m.clear();

    // news
    m.set("msg_speak", "$HIY【广播】%s：%s");
    m.set("msg_emote", "$HIY【广播】%s");
    m.set("msg_color", "$HIY");
    m.set("anonymous", "最新消息");
    m.set("wiz_use", 1);
    m.set("all_receive", 1);
    m.set("channel", "news");

    channels.set("news", m);
    m.clear();

    // wiz
    m.set("msg_speak", "%s%s：%s");
    m.set("msg_emote", "%s%s");
    m.set("msg_color", "$HIY");
    m.set("wiz_use", 1);
    m.set("all_receive", 1);
    m.set("channel", "wiz");

    channels.set("wiz", m);
    m.clear();

    // chat
    m.set("msg_speak", "$CYN【%s】%s：%s");
    m.set("msg_emote", "$CYN【闲聊】%s");
    m.set("msg_color", "$CYN");
    m.set("channel", "chat");

    channels.set("chat", m);
    m.clear();

    // ad
    m.set("msg_speak", "$HIR【交易】$CYN%s：%s");
    m.set("msg_emote", "$HIR【交易】$CYN%s");
    m.set("msg_color", "$CYN");
    m.set("channel", "ad");
    m.set("newbie", 1);

    channels.set("ad", m);
    m.clear();

    // noise
    m.set("msg_speak", "$HIM【市井】$CYN%s：%s");
    m.set("msg_emote", "$HIM【市井】$CYN%s");
    m.set("msg_color", "$CYN");
    m.set("channel", "noise");

    channels.set("noise", m);
    m.clear();

    // f1
    m.set("msg_speak", "$HIM【%s】$COM%s");
    m.set("msg_emote", "$HIM【%s】%s");
    m.set("msg_color", "$COM");
    m.set("channel", "f1");
    m.set("newbie", 1);

    channels.set("f1", m);
    m.clear();

    // f2
    m.set("msg_speak", "$HIG【%s】$COM%s");
    m.set("msg_emote", "$HIG【%s】%s");
    m.set("msg_color", "$COM");
    m.set("channel", "f2");
    m.set("newbie", 1);

    channels.set("f2", m);
    m.clear();

    // f3
    m.set("msg_speak", "$HIW【%s】$COM%s");
    m.set("msg_emote", "$HIW【%s】%s");
    m.set("msg_color", "$COM");
    m.set("channel", "f3");
    m.set("newbie", 1);

    channels.set("f3", m);
    m.clear();

    // f4
    m.set("msg_speak", "$HIW【%s】$COM%s");
    m.set("msg_emote", "$HIW【%s】%s");
    m.set("msg_color", "$COM");
    m.set("channel", "f4");
    m.set("newbie", 1);

    channels.set("f4", m);
    m.clear();

    // rumor
    m.set("msg_speak", "$HIM【谣言】%s：%s");
    m.set("msg_emote", "$HIM【谣言】%s");
    m.set("msg_color", "$HIM");
    m.set("use_mp", 1);
    m.set("anonymous", "某人");
    m.set("channel", "rumor");

    channels.set("rumor", m);
    m.clear();

    // party
    m.set("msg_speak", "$HIW【%s】%s: %s");
    m.set("msg_emote", "$HIW【%s】%s");
    m.set("msg_color", "$HIW");
    m.set("channel", "party");

    channels.set("party", m);
    m.clear();

    // corps
    m.set("msg_speak", "$YEL【%s】%s: %s");
    m.set("msg_emote", "$YEL【%s】%s");
    m.set("msg_color", "$YEL");
    m.set("newbie", 1);
    m.set("channel", "corps");

    channels.set("corps", m);
    m.clear();

    // answer
    m.set("msg_speak", "$YEL【考试】%s: %s");
    m.set("msg_color", "$YEL");
    m.set("newbie", 1);
    m.set("channel", "answer");

    channels.set("answer", m);
    m.clear();

    // 7, 从 Lua 配置加载聊天禁词
    load_ban_words_from_lua();
}

// 从 Lua 配置加载聊天禁词
void CChatChannel::load_ban_words_from_lua()
{
    m_chanbanword.clear();

    if (!g_LuaEngine) {
        LOG_ERROR("LoadBanWordsFromLua: Lua engine not initialized");
        return;
    }

    LOG_DEBUG(
        "LoadBanWordsFromLua: Attempting to load ban words via GetBanWords");

    // 通过统一数据接口获取禁词列表（封装在 LuaEngine 内部，不直接操作 Lua 栈）
    if (g_LuaEngine->get_ban_words(m_chanbanword)) {
        LOG_DEBUG("LoadBanWordsFromLua: Loaded %d banned words from Lua config",
                  (int)m_chanbanword.size());
    }
    else {
        LOG_ERROR(
            "LoadBanWordsFromLua: Failed to load banned words from data "
            "script");
    }
}

//---------------------------------------------------------
// check_channel_msg — 频率限制与禁词过滤
//---------------------------------------------------------
int CChatChannel::check_channel_msg(CChar* me, const char* verb,
                                    const char* chat, const char* block_key)
{
    char msg[255];

    // 1, 禁忌字眼.
    if (userp(me) && strcmp(verb, "noise") != 0) {
        for (const auto& word : m_chanbanword) {
            if (lstrchr((const unsigned char*)chat,
                        (const unsigned char*)word.c_str()))
                return notify_fail(
                    "对不起，因为您的语言里含有不文明字眼，系统拒绝发送。");
        }
    }

    // 2,防止频率过快.
    if (!wizardp(me) && EQUALSTR(me->querystr_temp("last_channel_msg"), chat))
        return notify_fail("用交谈频道说话请不要重复相同的讯息。");

    if (!wizardp(me)) {
        me->set_temp("last_channel_msg", chat);
        me->add_temp("channel_msg_cnt", 1);
        if (me->query_temp("channel_msg_cnt") > 5) {
            if (g_current_time - me->query_temp("chat_time")
                < 20)  // 20秒内发超过5条语言，被封。
            {
                if (block_key) {
                    me->set(block_key, g_current_time + 3600);
                    me->set_temp("no_say", 1);
                    do_channel(reinterpret_cast<CContainer*>(me), nullptr,
                               "rumor",
                               snprintf(msg, 255,
                                        "悔天鬼王拿出一张狗皮膏药，在小火炉上细"
                                        "细地煨热以后，“啪”地捂住了%s的嘴巴。",
                                        me->name(1)));
                    return notify_fail(
                        "由于发送了大量信息，你的公共频道被关闭一小时。");
                }
            }
            else {
                me->set_temp("chat_time", g_current_time);
                me->delete_temp("channel_msg_cnt");
            }
        }
    }

    return 1;
}

//---------------------------------------------------------
// filter_listener / group_listener / filter_list — 内部过滤
//---------------------------------------------------------
int CChatChannel::filter_listener(CUser* ppl, CMapping* ch)
{
    if (!ppl->environment()) return 0;

    if (ch->query("wiz_only")) return wizardp(ppl);

    return 1;
}

// 分组成员判定：同分组键同值；巫师恒通过（环境存在性由 filter_listener 管）
int CChatChannel::group_listener(CUser* ppl, const char* value, const char* key)
{
    if (wizardp(ppl)) return 1;
    return EQUALSTR(ppl->querystr(key), value);
}

// 过滤链表：门派频道按师徒系统分组（全体门派共用的机制，留 C++）；
// 数据集频道（如帮派）的分组判定经 hooks.channel_group 下放脚本。
// 返回 0 = 发言者未入组（notify_fail 已填写文案）；1 = list 已填充。
int CChatChannel::filter_list(CMapping* ch, CChar* me, const char* verb,
                              MAP2USER& list)
{
    std::string group_key;

    // 门派：师徒系统机制，分组键与"未入门派"判定留 C++
    if (strcmp(verb, "party") == 0) {
        if (!me->querystr("family/family_name")[0])
            return notify_fail("你还没有加入任何门派！");
        group_key = "family/family_name";
    }
    else {
        CLuaHookCall hook("channel_group");
        hook.add_str(verb).add_object(me);
        if (hook.call() && !hook.result_is_nil()) {
            std::string reject;
            if (hook.table_field_str("reject", reject))
                return notify_fail("%s", reject.c_str());
            hook.table_field_str("key", group_key);
        }
    }

    const bool grouping     = !group_key.empty();
    const char* group_value = grouping ? me->querystr(group_key.c_str()) : "";

    MAP2USER* ulist = users();
    iterator_user p;

    for (p = ulist->begin(); p != ulist->end(); p++) {
        if (!filter_listener(p->second, ch)) continue;
        if (grouping
            && !group_listener(p->second, group_value, group_key.c_str()))
            continue;

        // 增加关闭频道
        if (!ch->query("all_receive")) {
            CVector* tuned_ch = (p->second)->queryvec("channels");
            if (!tuned_ch) continue;
            if (tuned_ch->find(ch->querystr("channel")) == tuned_ch->end())
                continue;
        }

        list[p->first] = p->second;
    }
    return 1;
}

//---------------------------------------------------------
// do_channel — 核心广播
//---------------------------------------------------------
// 作为 chat* fjalfj 或 chat 你好
int CChatChannel::do_channel(CContainer* ob, CChar* who, const char* arg1,
                             const char* arg2)
{
    CChar* me;
    CChar tempChar;
    int emote = 0;
    char verb[40], chat[512], msg[512];

    strncpy(verb, arg1, 40)[39]   = 0;
    strncpy(chat, arg2, 480)[480] = 0;

    // 1, 判断发布消息者是个物件
    if (ob->is_character())
        me = static_cast<CChar*>(ob);
    else {
        me = &tempChar;
        me->set("channel_id", ob->querystr("channel_id"));
    }

    // 2, 分离动词，如果是带*的，将*去掉，标记其为一个表情。
    int len = strlen(verb) - 1;
    if (len < 0) return 0;
    if (verb[len] == '*' && len > 0) {
        emote     = 1;
        verb[len] = 0;
    }

    // 3, 判断动词是否属于合法频道
    CMapping* channel = channels.querymap(verb);
    if (!channel) return 0;

    // 4, 判断该频道是否支持表情
    if (emote && channel->undefinedp("msg_emote")) {
        tell_object(me, "对不起，这个频道不支持 emote 。");
        return 1;
    }

    // 5, 去掉右空格，如无说话内容，不理睬。
    len = strlen(chat) - 1;
    if (len < 0) return 1;

    while (len && chat[len] == ' ') chat[len--] = 0;

    if (!chat[0]) strcpy(chat, "...");

    // 6, 检查使用者是否有权利使用该频道。
    char block_key[20];

    snprintf(block_key, 20, "chblk/%s", verb);

    if (userp(me) && !wizardp(me)) {
        if (g_current_time < me->query("chblk/all"))
            return notify_fail("你所有的聊天频道都被关闭了！");

        if (g_current_time < me->query(block_key))
            return notify_fail("你的这个频道被关闭了！");
    }

    if (userp(me)) {
        if ((channel->query("wiz_only") || channel->query("wiz_use"))
            && !wizardp(me))
            return 0;

        if (channel->query("newbie") != 1 && me->query("age") < 15
            && !wizardp(me))
            return notify_fail("你的年纪太小了，只能用新手频道说话。");

        // check if same channel message but not wizard
        if (!check_channel_msg(me, verb, chat, block_key)) return 0;

        if (!wizardp(me) && channel->query("use_mp")) {
            if (me->query("mp") < 20)
                return notify_fail("你的内力不足以造谣！");

            me->receive_damage("mp", me->query("mp") / 2);
        }

        // 将该频道打开
        CMapping* m       = me->query_entire_dbase();
        CVector* tuned_ch = m->queryvec("channels");

        if (!tuned_ch) {
            CVector v;
            v.append(verb);
            me->set("channels", v);
        }
        else if (tuned_ch->find(verb) == tuned_ch->end()) {
            tuned_ch->append(verb);
        }
    }

    // 7, 转换为表情
    char* fmt = chat;
    int bHide = 0;

    if (emote && m_emote) {
        if (channel->query("interMain_emote"))
            fmt = m_emote->do_emote(me, who, chat, 3);
        else if (strcmp(verb, "rumor") == 0 && g_utils.random(10) < 8) {
            fmt   = m_emote->do_emote(me, who, chat, 2,
                                      channel->querystr("anonymous"));
            bHide = 1;
        }
        else
            fmt = m_emote->do_emote(me, who, chat, 1);

        if (!fmt || !fmt[0]) {
            // 只允许中文表情
            if (*((unsigned char*)&chat[0]) < 160) return 0;

            char tmp[512];

            strncpy(tmp, chat, 512);

            fmt = snprintf(chat, 512, "%s(%s)%s", me->name(), me->id(), tmp);

            if (strcmp(verb, "rumor") == 0) {
                if (userp(me))
                    fmt = snprintf(chat, 512, "%s%s",
                                   channel->querystr("anonymous"), tmp);
            }
        }
    }

    // 8, 获得发言者身份，如果是匿名频道，换作匿名。
    char mouren[40];

    strncpy(mouren, channel->querystr("anonymous"), 40)[39] = 0;

    if (mouren[0]) {
        // 造谣有20%的几率要破
        if (userp(me) && !emote)  // 修改表情有时连巫师也会骗过的BUG。
            if (g_utils.random(10) < 8)
                bHide = 1;
            else
                snprintf(mouren, 40, "%s(%s)", me->name(1), me->id(1));

        if (bHide)
            do_channel(ob, nullptr, "sys",
                       snprintf(msg, 512, "%s躲了起来造谣。", me->name(1)));
    }
    else {
        strncpy(mouren, me->querystr("channel_id"), 40)[39] = 0;
        if (userp(me) || !mouren[0]) {
            if (strcmp(me->name(0), me->name(1)) && emote)
                do_channel(ob, nullptr, "sys",
                           snprintf(msg, 512, "$HIW%s(%s)$HIW扮%s!",
                                    me->name(1), me->id(1), me->name()));

            snprintf(mouren, 40, "%s(%s)", me->name(1), me->id(1));
        }
    }

    // 9, 发送给接收该频道的用户

    MAP2USER list;
    if (!filter_list(channel, me, verb, list)) return 0;

    const char* wiz_name;
    int grant = me->query("grant");
    if (grant < 60)
        wiz_name = "$YEL【客座巫师】$HIY";
    else if (grant < 70)
        wiz_name = "$RED【见习巫师】$HIY";
    else if (grant < 80)
        wiz_name = "$HIG【天使】$HIY";
    else if (grant < 90)
        wiz_name = "$HIM【大天使】$HIY";
    else
        wiz_name = "$HIW【天神】$HIY";

    // 频道发言可由脚本接管（hooks.do_channel）：返回广播行数组则接管
    // 本次发言并逐行广播，nil 则交还内置频道分支
    CLuaHookCall hook("do_channel");
    if (hook.valid()) {
        hook.add_str(verb).add_object(me).add_str(fmt).add_str(mouren).add_str(
            channel->querystr("msg_speak"));
        if (hook.call() && !hook.result_is_nil()) {
            for (int i = 0, n = hook.table_str_count(); i < n; i++) {
                std::string line;
                if (hook.table_str_at(i, line))
                    message("channel:", line.c_str(), &list);
            }
            return 1;
        }
    }

    if (emote) {
        if (strcmp(verb, "party") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_emote"),
                             me->querystr("family/family_name"), fmt),
                    &list);
        else if (strcmp(verb, "corps") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_emote"),
                             load_room(me->querystr("corps/id"))->name(), fmt),
                    &list);
        else if (strcmp(verb, "wiz") == 0) {
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_emote"), wiz_name,
                             fmt),
                    &list);
        }
        else
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_emote"), fmt),
                    &list);
    }
    else {
        const char* corps_id = me->querystr("corps/id");

        if (strcmp(verb, "party") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_speak"),
                             me->querystr("family/family_name"), mouren, fmt),
                    &list);
        else if (strcmp(verb, "corps") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_speak"),
                             load_room(corps_id)->name(), mouren, fmt),
                    &list);
        else if (strcmp(verb, "wiz") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_speak"), wiz_name,
                             mouren, fmt),
                    &list);
        else if (strcmp(verb, "new") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_speak"),
                             corps_id[0] ? load_room(corps_id)->name() : "新手",
                             mouren, fmt),
                    &list);
        else if (strcmp(verb, "chat") == 0)
            message("channel:",
                    snprintf(msg, 512, channel->querystr("msg_speak"),
                             corps_id[0] ? load_room(corps_id)->name() : "闲聊",
                             mouren, fmt),
                    &list);
        else
            message(
                "channel:",
                snprintf(msg, 512, channel->querystr("msg_speak"), mouren, fmt),
                &list);
    }

    return 1;
}

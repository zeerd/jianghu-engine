/** @file EmoteSystem.cpp @brief EmoteSystem implementation. */

#include "EmoteSystem.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "Char.h"
#include "Global.h"
#include "LuaEngine.h"
#include "Message.h"
#include "User.h"

CEmoteSystem::~CEmoteSystem() {}

// 从 Lua 配置加载 emote 表情表（verb -> field -> 文案）
void CEmoteSystem::load_emotes_from_lua()
{
    std::map<std::string, std::map<std::string, std::string>> emotes;
    if (!g_LuaEngine || !g_LuaEngine->get_emote_map(emotes)) {
        LOG_ERROR("LoadEmotesFromLua: Failed to load emotes from Lua");
        return;
    }

    semote.clear();
    int count = 0;
    for (auto& kv : emotes) {
        const std::string& verb = kv.first;
        CMapping sub;
        for (auto& fkv : kv.second) {
            sub.set(fkv.first.c_str(), fkv.second.c_str());
        }
        // 深拷贝装入
        semote.set(verb.c_str(), sub);
        count++;
    }
    LOG_DEBUG("LoadEmotesFromLua: loaded %d emotes from Lua", count);
}

// 将 emote 动词下载到客户端
void CEmoteSystem::download_emote(CUser* me)
{
    char msg[512];
    CVector v;

    semote.keys(v);

    for (int i = 0; i < v.count(); i++) {
        snprintf(msg, 512, "&C=%ld&e=%s&", DOWNEMOTE, v[i]);
        me->send_command(msg, SEND_WAIT_BUFFERED);
    }
}

char* CEmoteSystem::do_emote(CChar* me, CChar* target, const char* verb,
                             int channel_emote, const char* who)
{
    static char msg[512];
    char tmp[512];
    string str;
    char my_gender[10], target_gender[10], msg_postfix[10];
    char target_name[40];
    CMapping* emote;

    if (!me->environment()) return nullptr;
    if (!verb[0]) return nullptr;

    if (!(emote = semote.querymap(verb))) return nullptr;

    if (!channel_emote && target && target->environment() != me->environment())
        return nullptr;

    if (target) {
        if (target == me) {
            strncpy(msg_postfix, "_self", 10);
            target = nullptr;
        }
        else {
            strncpy(msg_postfix, "_target", 10);

            strncpy(target_gender, target->querystr("gender"), 10);
            strncpy(target_name, target->name(), 40);

            if (userp(me) && userp(target)) {
                if (EQUALSTR(me->querystr("couple/id"),
                             target->querystr("id"))) {
                    snprintf(target_name, 40, "%s%s",
                             me->querystr("couple/gender"), target->name(1));
                }
            }
        }
    }
    else
        msg_postfix[0] = 0;

    strncpy(my_gender, me->querystr("gender"), 10);

    char key[20];

    snprintf(key, 20, "myself%s", msg_postfix);
    str = emote->querystr(key);
    if (str.length()) {
        strncpy(msg, str.c_str(), 512);
        replace_string(msg, "$N", who ? who : me->name(), 512);
        replace_string(msg, "$P", gender_self(my_gender), 512);
        replace_string(msg, "$S", query_self(me), 512);
        replace_string(msg, "$s", query_self_rude(me), 512);
        if (target) {
            replace_string(msg, "$C", query_self_close(me, target), 512);
            replace_string(msg, "$c", query_close(me, target), 512);
            replace_string(msg, "$R", query_respect(target), 512);
            replace_string(msg, "$r", query_rude(target), 512);
            replace_string(msg, "$n", target->name(), 512);
            replace_string(msg, "$p", gender_pronoun(target_gender), 512);
        }

        if (!channel_emote) tell_object(me, snprintf(tmp, 512, "$HIC%s", msg));
    }

    str = emote->querystr("target");
    if (target && str.length()) {
        strncpy(msg, str.c_str(), 512);
        replace_string(msg, "$N", who ? who : me->name(), 512);
        replace_string(msg, "$P", gender_pronoun(my_gender), 512);
        replace_string(msg, "$S", query_self(me), 512);
        replace_string(msg, "$s", query_self_rude(me), 512);
        replace_string(msg, "$C", query_self_close(me, target), 512);
        replace_string(msg, "$c", query_close(me, target), 512);
        replace_string(msg, "$R", query_respect(target), 512);
        replace_string(msg, "$r", query_rude(target), 512);
        replace_string(msg, "$n", target->name(), 512);
        replace_string(msg, "$p", gender_self(target_gender), 512);

        if (!channel_emote)
            tell_object(target, snprintf(tmp, 512, "$HIC%s", msg));
    }

    snprintf(key, 20, "others%s", msg_postfix);
    str = emote->querystr(key);
    if (str.length()) {
        char tmp[512];
        strncpy(msg, str.c_str(), 512);
        replace_string(msg, "$N",
                       who ? who
                           : (userp(me) ? snprintf(tmp, 255, "%s(%s)",
                                                   me->name(), me->id())
                                        : snprintf(tmp, 255, "%s", me->name())),
                       512);
        replace_string(msg, "$P", gender_pronoun(my_gender), 512);
        replace_string(msg, "$S", query_self(me), 512);
        replace_string(msg, "$s", query_self_rude(me), 512);
        if (target) {
            replace_string(msg, "$C", query_self_close(me, target), 512);
            replace_string(msg, "$c", query_close(me, target), 512);
            replace_string(msg, "$R", query_respect(target), 512);
            replace_string(msg, "$r", query_rude(target), 512);
            replace_string(msg, "$n", target->name(), 512);
            replace_string(msg, "$p", gender_pronoun(target_gender), 512);
        }

        if (!channel_emote) {
            message_vision_room(me, "chat", snprintf(tmp, 512, "$HIC%s", msg),
                                me, target);
        }
        else
            return msg;
    }

    // Let NPC know we are doing emote on him.
    if (target) target->relay_emote(me, verb);

    return const_cast<char*>("");
}

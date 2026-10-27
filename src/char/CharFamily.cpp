/**
 * @file CharFamily.cpp
 * @brief implementation of CChar (CharFamily). */

#include <windows.h>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Skill.h"
#include "CmdHandle.h"
#include "Room.h"
#include "Condition.h"
#include "Channel.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "AutoFight.h"
#include "CombatConfig.h"
#include "ServerConfig.h"
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"

// 是师徒关系
int CChar::is_apprentice_of(CChar* me)
{
    const char* bangpai = querystr("family/family_name");

    if (!bangpai[0]) return 0;

    if (EQUALSTR(querystr("family/master_id"), me->querystr("id"))
        && EQUALSTR(bangpai, me->querystr("family/family_name")))
        return 1;

    return 0;
}

// 收徒
int CChar::recruit_apprentice(CChar* me)
{
    CMapping *my_family, family;

    if (me->is_apprentice_of(this)) return 0;
    if (!(my_family = querymap("family"))) return 0;

    family.set("master_id", id(1));
    family.set("master_name", name(1));
    family.set("family_name", my_family->querystr("family_name"));
    family.set("generation", my_family->query("generation") + 1);
    family.set("enter_time", g_current_time);

    if (is_user(this)) {
        family.set("last_exp", me->query_exp());
        family.set("user", 1);
    }

    me->release_master();

    me->set("family", family);
    me->assign_apprentice("弟子", 0);

    // 更新me的菜单
    download(me);

    return 1;
}

void CChar::attempt_apprentice(CChar* me) {}

void CChar::create_family(char* family_name, int generation, char* title)
{
    CMapping family;

    family.set("family_name", family_name);
    family.set("generation", generation);
    set("family", family);

    // priv = -1 for ALL privileges.
    assign_apprentice(title, -1);
}

void CChar::send_off() {}

void CChar::assign_apprentice(const char* title, int privs)
{
    CMapping* family;
    char msg[255], tmp[255];

    if (!(family = querymap("family"))) return;

    family->set("title", title);
    family->set("privs", privs);

    if (query_attr(IS_USER) || !querystr("title")[0]) {
        if (family->query("generation") == 1) {
            snprintf(msg, 255, "%s开山祖师", family->querystr("family_name"));
        }
        else {
            snprintf(msg, 255, "%s第%s代%s", family->querystr("family_name"),
                     chinese_number(family->query("generation"), tmp),
                     family->querystr("title"));
        }

        set_title(msg);
    }
}

void CChar::set_title(const char* title)
{
    set("title", title);
    update_me();
}

int CChar::prevent_learn(CChar* me, const char* skill)
{
    int betrayer;

    if ((betrayer = me->query("betrayer"))) {
        if (CServerConfig::no_betrayer_valid()) {
            message_vision(
                "$N神色间似乎对$n不是十分信任，也许是想起$p从前背叛师门的事情 "
                "...。",
                this, me);
            return 1;
        }
        else {
            if (me->query_skill(skill, 1)
                >= query_skill(skill, 1) - betrayer * 20) {
                message_vision(
                    "$N神色间似乎对$n不是十分信任，也许是想起$"
                    "p从前背叛师门的事情 "
                    "...。",
                    this, me);
                say("嗯 .... 师父能教你的都教了，其他的你自己练吧。", me);
                send_menu(me);
                return 1;
            }
        }
    }

    if (!me->is_apprentice_of(this)
        && query_skill(skill, 1) <= me->query_skill(skill, 1) * 3) {
        say(g_channel.do_emote(this, 0, "hmm", 1), me);
        say(g_channel.do_emote(this, me, "pat", 1), me);
        say("虽然你是我门下的弟子，可是并非我的嫡传弟子 ....", me);
        say("我只能教你这些粗浅的本门功夫，其他的还是去找你师父学吧。", me);
        send_menu(me);
        return 1;
    }

    return 0;
}

int CChar::recognize_apprentice(CChar* me) { return 0; }

void CChar::set_leader(CChar* me)
{
    if (me == this)
        LOG_ERROR("set_leader: Cannot set this_object() as leader.");

    m_leader = me;
}

CChar* CChar::query_leader() { return m_leader; }

int CChar::follow_me(CChar* me, char* dir)
{
    char msg[80];
    if (!is_living(this) || me == this) return 0;

    if (me == m_leader || (query("pursuer") && is_killing(me))) {
        return command(snprintf(msg, 80, "go 0 %s", dir));
    }

    return 0;
}

void CChar::relay_emote(CChar* me, string verb) {}

void CChar::relay_say(CChar* me, char* msg) {}

// 释放原来师父的收徒记录
void CChar::release_master()
{
    char msg[80];

    if (!query("family/user")) return;  // 非玩家师父,不需要修改

    const char* master = _M.querystr("family/master_id");
    CMapping* ptr      = g_channel.query_recruit(master);
    if (!ptr) return;

    g_channel.update_recruit(master, id(1), name(1),
                             ptr->query(snprintf(msg, 80, "%s/exp", id(1))), 0);
}

// 计算当前徒弟数目
int CChar::recruit_num()
{
    int count     = 0;
    CMapping* ptr = g_channel.query_recruit(id(1));

    if (ptr) {
        CVector v;

        ptr->keys(v);

        for (int i = 0; i < v.count(); i++) {
            CMapping* m = ptr->querymap(v[i]);
            count += m->query("apprentice");
        }
    }

    return count;
}

// 杀死某玩家时呼叫
void CChar::finish_kill(CChar* me) {}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:set_leader(leader) - 设置跟随队长（CChar）
int l_set_leader(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CMainObject* lo = get_object(L, 2);
    CChar* leader   = lo ? dynamic_cast<CChar*>(lo) : NULL;
    ch->set_leader(leader);
    return 0;
}

// self:recruit_num() -> number
int l_recruit_num(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    lua_pushinteger(L, c->recruit_num());
    return 1;
}

// self:release_master()
int l_release_master(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    c->release_master();
    return 0;
}

// self:is_apprentice_of(me) -> number
int l_is_apprentice_of(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    lua_pushinteger(L, me ? c->is_apprentice_of(me) : 0);
    return 1;
}

// self:recruit_apprentice(me) -> number
int l_recruit_apprentice(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    lua_pushinteger(L, me ? c->recruit_apprentice(me) : 0);
    return 1;
}

// self:create_family(family, generation, title)
int l_create_family(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    const char* family = luaL_checkstring(L, 2);
    int generation     = (int)luaL_checkinteger(L, 3);
    const char* title  = luaL_optstring(L, 4, "弟子");
    char famBuf[128], titBuf[128];
    strncpy(famBuf, family, sizeof(famBuf) - 1);
    famBuf[sizeof(famBuf) - 1] = 0;
    strncpy(titBuf, title, sizeof(titBuf) - 1);
    titBuf[sizeof(titBuf) - 1] = 0;
    c->create_family(famBuf, generation, titBuf);
    return 0;
}

// self:assign_apprentice(title, privs)
int l_assign_apprentice(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    const char* title = luaL_checkstring(L, 2);
    int privs         = (int)luaL_checkinteger(L, 3);
    char titBuf[128];
    strncpy(titBuf, title, sizeof(titBuf) - 1);
    titBuf[sizeof(titBuf) - 1] = 0;
    c->assign_apprentice(titBuf, privs);
    return 0;
}

// self:recognize_apprentice(me) -> number
int l_recognize_apprentice(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    lua_pushinteger(L, me ? c->recognize_apprentice(me) : 0);
    return 1;
}

// self:prevent_learn(me, skill) -> number
int l_prevent_learn(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    const char* skill = luaL_optstring(L, 3, "");
    lua_pushinteger(L, me ? c->prevent_learn(me, skill) : 0);
    return 1;
}

// self:attempt_apprentice(me)
int l_attempt_apprentice(lua_State* L)
{
    CChar* c = dynamic_cast<CChar*>(get_object(L, 1));
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (me) c->attempt_apprentice(me);
    return 0;
}

// query_recruit(id) -> table|nil -
// 查询某人收徒记录（key=弟子id，值={name,apprentice,exp}）
int l_query_recruit(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    CMapping* m    = g_channel.query_recruit(id);
    if (!m) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    CVector keys;
    m->keys(keys);
    for (int i = 0; i < keys.count(); i++) {
        const char* key = keys[i];
        if (!key) continue;
        CMapping* p = m->querymap(key);
        lua_newtable(L);
        if (p) {
            const char* nm = p->querystr("name");
            lua_pushstring(L, nm ? nm : "");
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, p->query("apprentice"));
            lua_setfield(L, -2, "apprentice");
            lua_pushinteger(L, p->query("exp"));
            lua_setfield(L, -2, "exp");
        }
        lua_setfield(L, -2, key);
    }
    return 1;
}

// update_recruit(master, id, name, exp, apprentice)
int l_update_recruit(lua_State* L)
{
    const char* master = luaL_checkstring(L, 1);
    const char* id     = luaL_checkstring(L, 2);
    const char* name   = luaL_checkstring(L, 3);
    int32_t exp        = (int32_t)luaL_checkinteger(L, 4);
    int apprentice     = (int)luaL_checkinteger(L, 5);
    char nameBuf[128];
    strncpy(nameBuf, name, sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = 0;
    g_channel.update_recruit(master, id, nameBuf, exp, apprentice);
    return 0;
}

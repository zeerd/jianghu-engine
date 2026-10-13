/** @file Channel.cpp @brief CChannel facade — delegates to subsystems. */

#include "Channel.h"

#include "Global.h"

//---------------------------------------------------------
// Construction/Destruction — reference members alias subsystem data
//---------------------------------------------------------
CChannel::CChannel()
    : voted_players(m_voting.voted_players),
      votes_casted(m_voting.votes_casted),
      banlist(m_banlist.banlist),
      loginlist(m_banlist.loginlist),
      loginlimit(m_banlist.loginlimit)
{
    // Wire EmoteSystem into ChatChannel (do_channel needs do_emote)
    m_chat.set_emote_system(&m_emote);
}

CChannel::~CChannel() { release_me(); }

CBanlistManager::~CBanlistManager() {}
CChatChannel::~CChatChannel() {}
CVotingSystem::~CVotingSystem() {}

//---------------------------------------------------------
// Lifecycle
//---------------------------------------------------------
void CChannel::create()
{
    release_me();

    set("channel_id", "频道精灵");

    // Initialize each subsystem
    m_banlist.clear();
    m_voting.clear();
    m_chat.create();
    m_emote.load_emotes_from_lua();
}

void CChannel::release_me() { m_snoop.release_me(); }

//---------------------------------------------------------
// ChatChannel delegation
//---------------------------------------------------------
int CChannel::do_channel(CContainer* me, CChar* who, const char* arg1,
                         const char* arg2)
{
    return m_chat.do_channel(me, who, arg1, arg2);
}

int CChannel::check_channel_msg(CChar* me, const char* verb, const char* msg,
                                const char* block_key)
{
    return m_chat.check_channel_msg(me, verb, msg, block_key);
}

//---------------------------------------------------------
// EmoteSystem delegation
//---------------------------------------------------------
char* CChannel::do_emote(CChar* me, CChar* target, const char* verb,
                         int channel_emote, const char* who)
{
    return m_emote.do_emote(me, target, verb, channel_emote, who);
}

void CChannel::download_emote(CUser* me) { m_emote.download_emote(me); }

//---------------------------------------------------------
// SnoopSystem delegation
//---------------------------------------------------------
void CChannel::snoop(CChar* me, CChar* victim) { m_snoop.snoop(me, victim); }

CChar* CChannel::query_snooping(CChar* me)
{
    return m_snoop.query_snooping(me);
}

void CChannel::do_snooping(CChar* victim, char* msg)
{
    m_snoop.do_snooping(victim, msg);
}

void CChannel::remove_snoop(CChar* me) { m_snoop.remove_snoop(me); }

//---------------------------------------------------------
// RelationshipTracker delegation
//---------------------------------------------------------
void CChannel::update_relative(const char* me, const char* target, int32_t pt,
                               int flag)
{
    m_relationship.update_relative(me, target, pt, flag);
}

CMapping* CChannel::query_relative(const char* me, const char* target)
{
    return m_relationship.query_relative(me, target);
}

void CChannel::update_recruit(const char* master, const char* id,
                              const char* name, int32_t exp, int apprentice)
{
    m_relationship.update_recruit(master, id, name, exp, apprentice);
}

CMapping* CChannel::query_recruit(const char* id)
{
    return m_relationship.query_recruit(id);
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// do_channel(me, who, channel, msg) - 通过全局频道广播消息
int l_do_channel(lua_State* L)
{
    CContainer* me = NULL;
    if (lua_isuserdata(L, 1)) me = dynamic_cast<CContainer*>(get_object(L, 1));
    if (!me) me = &g_channel;  // nil 时回退到全局频道对象
    CChar* who = NULL;
    if (lua_isuserdata(L, 2)) who = dynamic_cast<CChar*>(get_object(L, 2));
    const char* channel = luaL_checkstring(L, 3);
    const char* msg     = luaL_checkstring(L, 4);
    g_channel.do_channel(me, who, channel, msg);
    return 0;
}

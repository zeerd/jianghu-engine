/**
 * @file Char.cpp
 * @brief CChar core lifecycle, heartbeat and basic queries.
 */

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
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"
#include "Utils.h"

#include "Sql.h"

#include "LuaBindEnums.h"

CCmdHandle CommandHandle;

CChar::CChar()
{
    // DEFAULT:
    m_busy_count = 0;
    m_busy_func  = 0;
    m_interrupt  = 0;
    m_revive     = 0;

    m_tick         = 0;
    m_tick_two     = 0;
    m_wait_loading = 0;
    m_tick_cnd     = 0;

    set("gender", "男性");

    set_temp("max_items", 100);

    m_leader     = 0;
    m_save_count = 0;
}

CChar::~CChar() { m_equip_list.clear(); }

int32_t CChar::query_attr(int attr)
{
    switch (attr) {
        case IS_CHAR:
            return 1;

        case CAN_GET:
            return !query_attr(IS_LIVING);

        case IS_LIVING: {
            if (querystr_temp("disabled")[0]) return 0;
            return 1;
        }
        case IS_BUSY:
            return m_busy_func != NULL;
    }

    return CContainer::query_attr(attr);
}

CChar::DTEquipList* CChar::get_equip_list() { return &m_equip_list; }

// 返回装备位置的武器
int32_t CChar::query_equip(int pos)
{
    iterator_equip p;

    p = m_equip_list.find(pos);
    if (p == m_equip_list.end()) return 0;
    return p->second;
}

// 心跳
void CChar::heart_beat()
{
    int wimpy_ratio;
    int32_t object_id = m_object_id;
    if (!this) return;

    if (!environment()) return;
    if (is_loading()) {
        m_wait_loading++;
        if (m_wait_loading > 5 && userp(this))  // 5秒后自动切入地图
        {
            // 让他进入
            command("loc 0 0 0");
        }

        return;  // 载入地图时暂停心跳
    }

    m_wait_loading = 0;

    CContainer::heart_beat();

    // 判断是否已经在延迟函式中销毁
    if (g_last_destruct_object == object_id) return;

    if (m_revive) {
        if (!--m_revive) revive(0);
    }

    if (query("eff_hp") < 0) {
        die();
        return;
    }

    if (query("hp") < 0) {
        if (!query_attr(IS_LIVING))
            die();
        else
            unconcious();
        return;  // 必须返回，在Die中，物件可能已被销毁。
    }

    if (query("combat_exp") < -2000000000) {
        set("combat_exp", LONGMAX);
    }

    if (userp(this)) {
        m_save_count++;
        if (m_save_count >= 100) {
            m_save_count = g_utils.random(40);
            if (userp(this)) save();  // auto save
        }

        send_off();
    }

    if (is_busy()) {
        continue_action();
        // We don't want heart beat be halt eventually, so return here.
        return;
    }

    // 战斗中逃跑
    if (is_fighting() && (wimpy_ratio = query("env/wimpy"))
        && (query("hp") * 100 / query("max_hp") <= wimpy_ratio)) {
        command("halt");
    }

    // NPC Random Action
    if (query_attr(IS_NPC)) {
        // 有可能已经释放
        if (static_cast<CNpc*>(this)->self_action()) {
            if (object_id == g_last_destruct_object) return;
        }
    }

    if (m_tick--)
        return;
    else {
        m_tick = 5 + g_utils.random(5);
        m_tick_two++;
    }

    int cnd_flag = update_condition();

    if (m_tick_two < 8)  // 40 ~ 80 s发一次
        return;

    m_tick_two = 0;

    if (heal_up() && !(cnd_flag & CND_NO_HEAL_UP)) {
        send_state(false);
    }
    else if (!is_fighting() && !m_callout_list.size() && !query("chat_chance")
             && !query_attr(IS_USER))

        set_heart_beat(0);

    if (!query_attr(IS_USER)) return;

    time_t offtime;

    if ((offtime = query_temp("net_dead")))  // 掉线了
    {
        if (g_current_time - offtime > 600  // 离线超过10分钟，将退出游戏。
        ) {
            LOG_INFO("掉线超时退出: %s(id:%s) 掉线%ld秒", name(1), id(1),
                     g_current_time - offtime);
            command("quit");
            return;
        }
    }

    static_cast<CUser*>(this)->update_age();

    if (g_combat_config.kick_idle()) {
        if (static_cast<CUser*>(this)->query_idle() > IDLE_TIMEOUT
            && !wizardp(this)) {
            static_cast<CUser*>(this)->user_dump();
        }
    }
}

// 初始化
void CChar::setup()
{
    set_heart_beat(1);
    m_tick = 5 + g_utils.random(10);
    enable_player();

    setup_char(this);
}

int CChar::enable_player()
{
    delete_temp("disabled");
    send_state();
    return 1;
}

int CChar::disable_player(const char* szType)
{
    set_temp("disabled", szType);
    send_state();
    return 1;
}

// 发送指令
int CChar::command(char* arg) { return CommandHandle.command(arg, this); }

// 装备位置debug function
char* CChar::wield_position(int pos)
{
    static char sz[][5]
        = {"无", "头",   "左耳", "右耳", "耳",   "颈",   "左手", "右手",
           "手", "左腕", "右腕", "腕",   "铠甲", "腰带", "靴子", "斗篷"};

    return sz[pos];
}

// 重新设定等级
void CChar::reset_level()
{
    if (!query_attr(IS_USER)) return;

    while (query_valid_exp() < 0) {
        if (query("level") == 1) break;

        add("level", -1);
    }

    setup_char(this);
}

// 心跳时恢复
int CChar::heal_up()
{
    int update_flag = 0;
    int32_t addhp   = 0;
    int32_t m_hp, m_hp_eff, m_hp_max;

    m_hp     = query("hp");
    m_hp_eff = query("eff_hp");
    m_hp_max = query("max_hp");

    addhp = query("con") / 3;
    m_hp  = add("hp", addhp);
    if (m_hp >= m_hp_eff) {
        set("hp", m_hp_eff);
        if (m_hp_eff < m_hp_max) {
            add("eff_hp", 1);
            update_flag++;
        }
    }
    else
        update_flag++;

    return update_flag;
}

int CChar::is_busy() { return m_busy_func || m_busy_count; }

int CChar::is_fighting(CContainer* enemy)
{
    if (!query_temp("fight/is_fighting")) return 0;

    if (enemy) {
        if (enemy->environment() != environment()) return 0;
        if (enemy->query_temp("fight/is_fighting")
            && enemy->query_temp("fight/fight_team")
                   != query_temp("fight/fight_team"))
            return 1;

        return 0;
    }

    return 1;
}

void CChar::clear_condition()
{
    del("condition");
    delete_temp("condition");
}

int32_t CChar::query_condition(const char* cnd)
{
    char key[40];

    snprintf(key, 40, "condition/%s", cnd);
    return _M.query(key);
}

int CChar::is_killing(CChar* ob)
{
    char id[20];
    return (m_killers.find(snprintf(id, 20, "%ld", ob->object_id()))
            != m_killers.end());
}

void CChar::remove_faint(CContainer* me, int32_t param1, int32_t param2)
{
    me->delete_temp("last_fainted_from");
}

int CChar::query_team() { return query_temp("fight/fight_team"); }

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:is_fighting() -> boolean
int l_is_fighting(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    lua_pushboolean(L, ch->is_fighting());
    return 1;
}

// self:query_condition(cnd) -> number
int l_query_condition(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    const char* cnd = luaL_checkstring(L, 2);
    int32_t dur     = ch->query_condition(cnd);
    lua_pushinteger(L, dur);
    return 1;
}

// self:clear_condition()
// 调用 CChar::clear_condition（清除所有 condition 及临时 condition）
int l_clear_condition(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;

    ch->clear_condition();
    return 0;
}

// self:is_busy() -> boolean
int l_is_busy(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushboolean(L, ch->is_busy());
    return 1;
}

// self:reset_level() - 重新设定等级（经验不足时逐级下降直到符合）
int l_reset_level(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->reset_level();
    return 0;
}

// self:is_killing(target) -> boolean
int l_is_killing(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    CChar* target = NULL;
    if (lua_isuserdata(L, 2)) target = dynamic_cast<CChar*>(get_object(L, 2));
    lua_pushboolean(L, target && ch->is_killing(target));
    return 1;
}

// self:command(cmd) -> boolean - 执行命令并返回是否成功
// （对应 C++ "if (command(...))" 的返回值判定语义，脚本可直接
//   "if self:command(...) then" 使用；忽略返回值的脚本不受影响）
int l_command(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) {
        lua_pushboolean(L, 0);
        return 1;
    }
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* cmd = luaL_checkstring(L, 2);
    char buf[256];
    strncpy(buf, cmd, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    lua_pushboolean(L, ch->command(buf) != 0);
    return 1;
}

// self:query_team() -> number - 所在战斗队伍（CFightRoom::FIGHT_TEAM_A/B）
int l_query_team(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->query_team());
    return 1;
}

// self:disable_player() -> void
int l_disable_player(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    const char* msg = luaL_checkstring(L, 2);
    ch->disable_player(msg);
    return 0;
}

// self:enable_player() -> void
int l_enable_player(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->enable_player();
    return 0;
}

// self:setup() - 重新初始化（CRoom/CChar）
int l_setup(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    if (CRoom* room = dynamic_cast<CRoom*>(obj)) {
        room->setup();
        return 0;
    }
    if (CNpc* npc = dynamic_cast<CNpc*>(obj)) {
        npc->setup();
        return 0;
    }
    if (CChar* ch = dynamic_cast<CChar*>(obj)) {
        ch->setup();
        return 0;
    }
    return 0;
}

// Char.h 动作枚举 → Lua 全局常量
static const LuaEnumEntry kCharActionEnums[] = {
    {"DO_WIELD", CChar::DO_WIELD}, {"DO_UNWIELD", CChar::DO_UNWIELD},
    {"DO_USE", CChar::DO_USE},     {"DO_LIST", CChar::DO_LIST},
    {"DO_TALK", CChar::DO_TALK},   {nullptr, 0},
};

// Char.h condition 标志位（全局宏）→ Lua 全局常量
static const LuaEnumEntry kCharCondEnums[] = {
    {"CND_CONTINUE", CND_CONTINUE},
    {"CND_NO_HEAL_UP", CND_NO_HEAL_UP},
    {nullptr, 0},
};

void lua_bind_reg_char_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kCharActionEnums);
    lua_bind_reg_enums(L, kCharCondEnums);
}

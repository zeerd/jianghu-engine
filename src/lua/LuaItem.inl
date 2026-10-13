// LuaItem.inl : 由 Lua 脚本定义的 Item 模板实现
// 注意：此文件被 LuaItem.h 包含（模板类），不作为独立编译单元
//
//---------------------------------------------------------

#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaItem.h"

#include <lua.hpp>
#include <vector>

// Item 实例注册表的非模板实现见 LuaReload.cpp

template <typename T>
static void c_lua_item_t_reload_bridge(void* p)
{
    static_cast<CLuaItemT<T>*>(p)->reload_script();
}

template <typename T>
CLuaItemT<T>::CLuaItemT(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
    CLuaItemInstanceRegistry::register_entry(this, m_script_name.c_str(),
                                             c_lua_item_t_reload_bridge<T>);
}

template <typename T>
CLuaItemT<T>::~CLuaItemT()
{
    CLuaItemInstanceRegistry::unregister(this);
}

// 刷新脚本引用并重新执行 create()（热重载用）
template <typename T>
void CLuaItemT<T>::reload_script()
{
    LuaScript* script
        = g_LuaEngine->get_script(m_script_name.c_str(), LUA_SCRIPT_ITEM);
    if (!script || script->ref == LUA_NOREF) return;
    m_script_ref = script->ref;

    // 重新执行 create() 使脚本改动立即生效
    create();
}

// 调用 Lua 脚本的 create 函数
template <typename T>
void CLuaItemT<T>::create(int flag)
{
    LOG_TRACE("IN");
    do {
        LOG_DEBUG("m_scriptName=%s", m_script_name.c_str());
        // 加载脚本（若已加载且文件修改则自动热重载）
        m_script_ref
            = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_ITEM);
        if (m_script_ref == LUA_NOREF) {
            // 脚本加载失败，回退到基类
            T::create(flag);
            return;
        }

        // 调用脚本的 create(self, nFlag)
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "create");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            T::create(flag);
            return;
        }

        lua_bind_push_object(L, this);
        lua_pushinteger(L, flag);

        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            LOG_ERROR("Lua item create failed [%s]: %s", m_script_name.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  // 弹出 table

        // 脚本声明的持久化路由（save_table/save_key）镜像进 dbase：
        // 存/取/删按 dbase 字段路由，load() 之后依然有效。
        std::string save_table, save_key;
        g_LuaEngine->get_script_str_field(
            m_script_name.c_str(), LUA_SCRIPT_ITEM, "save_table", save_table);
        g_LuaEngine->get_script_str_field(
            m_script_name.c_str(), LUA_SCRIPT_ITEM, "save_key", save_key);
        if (!save_table.empty()) {
            set("save_table", save_table.c_str());
            // 声明专属存盘表即可存盘（create 期生效，早于 load/save 时序）
            set("can_save", 1);
        }
        if (!save_key.empty()) {
            set("save_key", save_key.c_str());
            // 专属实例键（如 unique_id）为空时自动生成 UUID
            if (querystr(save_key.c_str())[0] == '\0') {
                UUID uuid;
                ::UuidCreate(&uuid);
                RPC_CSTR szUuid = NULL;
                if (::UuidToStringA(&uuid, &szUuid) == RPC_S_OK) {
                    set(save_key.c_str(), (char*)szUuid);
                    ::RpcStringFreeA(&szUuid);
                }
            }
        }
    } while (0);
    LOG_TRACE("OUT");
}

// 调用 Lua 脚本的 do_use 函数
template <typename T>
int CLuaItemT<T>::do_use(CChar* me, CContainer* target)
{
    if (m_script_ref == LUA_NOREF) return T::do_use(me, target);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "do_use");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::do_use(me, target);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_bind_push_object(L, target);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item do_use failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::do_use(me, target);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    if (result == 1 && me) me->send_state(false);
    return result;
}

// 调用 Lua 脚本的 do_look 函数
template <typename T>
int CLuaItemT<T>::do_look(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::do_look(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "do_look");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::do_look(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item do_look failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::do_look(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 Can_Use 函数
template <typename T>
int CLuaItemT<T>::can_use()
{
    if (m_script_ref == LUA_NOREF) return T::can_use();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "Can_Use");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::can_use();
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item Can_Use failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::can_use();
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 Can_Equip 函数
// 注意：Can_Equip 只在 CEquip 存在，CItem 没有，所以不调用 T::Can_Equip
template <typename T>
int CLuaItemT<T>::can_equip(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return 1;  // 默认允许装备

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "Can_Equip");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;  // 脚本没有 Can_Equip，默认允许
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item Can_Equip failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return 1;
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 hit_ob 函数
template <typename T>
const char* CLuaItemT<T>::hit_ob(CChar* me, CChar* victim, int32_t damage_bonus)
{
    if (m_script_ref == LUA_NOREF) return T::hit_ob(me, victim, damage_bonus);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "hit_ob");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::hit_ob(me, victim, damage_bonus);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_bind_push_object(L, victim);
    lua_pushinteger(L, damage_bonus);

    if (lua_pcall(L, 4, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item hit_ob failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::hit_ob(me, victim, damage_bonus);
    }

    lua_pop(L, 2);
    return "";
}

// 调用 Lua 脚本的 query_autoload 函数
template <typename T>
int CLuaItemT<T>::query_autoload()
{
    if (m_script_ref == LUA_NOREF) return T::query_autoload();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_autoload");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::query_autoload();
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item query_autoload failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::query_autoload();
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 handle_action 函数
template <typename T>
int CLuaItemT<T>::handle_action(char* comm, CChar* me, char* arg)
{
    if (m_script_ref == LUA_NOREF) return T::handle_action(comm, me, arg);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "handle_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::handle_action(comm, me, arg);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, comm ? comm : "");
    lua_pushstring(L, arg ? arg : "");

    if (lua_pcall(L, 4, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item handle_action failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::handle_action(comm, me, arg);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 handle_action4 函数
template <typename T>
int CLuaItemT<T>::handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (m_script_ref == LUA_NOREF)
        return call_base_handle_action4(has_handle_action4<T>(), comm, me, arg,
                                        tar);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "handle_action4");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return call_base_handle_action4(has_handle_action4<T>(), comm, me, arg,
                                        tar);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, comm ? comm : "");
    lua_pushstring(L, arg ? arg : "");
    lua_pushstring(L, tar ? tar : "");

    if (lua_pcall(L, 5, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item handle_action4 failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return call_base_handle_action4(has_handle_action4<T>(), comm, me, arg,
                                        tar);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 owner_is_killed 函数（若存在）
template <typename T>
void CLuaItemT<T>::owner_is_killed(CChar* killer)
{
    if (m_script_ref == LUA_NOREF) {
        call_base_owner_is_killed(has_owner_is_killed<T>(), killer);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "owner_is_killed");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        call_base_owner_is_killed(has_owner_is_killed<T>(), killer);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, killer);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua item owner_is_killed failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        call_base_owner_is_killed(has_owner_is_killed<T>(), killer);
        return;
    }
    lua_pop(L, 2);
}

// 调用 Lua 脚本的 do_action 函数（若存在）
template <typename T>
int CLuaItemT<T>::do_action(int act, CChar* me)
{
    if (m_script_ref == LUA_NOREF) return call_base_do_action(act, me);

    // 若 me 为空，从环境解析出持有者（与 C++ 武器 Do_Action 一致）
    if (!me && environment() && environment()->is_character())
        me = (CChar*)environment();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "do_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return call_base_do_action(act, me);
    }

    lua_bind_push_object(L, this);
    lua_pushinteger(L, act);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua item do_action failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return call_base_do_action(act, me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 on_moved 函数（若存在）
template <typename T>
void CLuaItemT<T>::on_moved(CContainer* me)
{
    if (m_script_ref == LUA_NOREF) {
        // 基类 CItem/CEquip 均有空实现，直接调用（安全）
        T::on_moved(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "on_moved");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::on_moved(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua item on_moved failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        T::on_moved(me);
        return;
    }
    lua_pop(L, 2);
}

// 调用 Lua 脚本的 init 函数
// 回退语义：脚本定义了 init(self) 时由脚本完全接管（需要基类行为时
// 脚本显式调用 self:base_init()）；脚本未定义/未加载时执行基类 T::init()
// （CItem 的反复制检查/重量同步）。
template <typename T>
void CLuaItemT<T>::init()
{
    if (m_script_ref != LUA_NOREF) {
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);

        lua_getfield(L, -1, "init");
        if (lua_isfunction(L, -1)) {
            lua_bind_push_object(L, this);
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                LOG_ERROR("Lua item init failed [%s]: %s",
                          m_script_name.c_str(), lua_tostring(L, -1));
                lua_pop(L, 1);
            }
            lua_pop(L, 1);  // 弹出 script table
            return;
        }
        lua_pop(L, 2);  // 弹出 nil + script table
    }

    // 脚本未定义 init（或脚本未加载）：回退 C++ 基类 init
    T::init();
}

// 供 Lua 调用的装备动作包装
template <typename T>
int CLuaItemT<T>::lua_do_wield()
{
    return call_base_do_wield(has_do_wield<T>());
}

template <typename T>
int CLuaItemT<T>::lua_do_un_wield()
{
    return call_base_do_unwield(has_do_wield<T>());
}

// 供 Lua 调用的默认动作（绕过 Lua 重写，直接调用基类 Do_Action）
template <typename T>
int CLuaItemT<T>::lua_do_action_base(int act, CChar* me)
{
    if (!me && environment() && environment()->is_character())
        me = (CChar*)environment();
    return call_base_do_action(act, me);
}

// 重写 Query：从 Lua 脚本属性读取 CAN_COMBINE 标志
// 当脚本定义了 can_combine 属性时，返回其布尔值；否则回退到基类 T::Query()
template <typename T>
int32_t CLuaItemT<T>::query_attr(int attr)
{
    if (attr == CAN_SAVE) {
        return query("can_save");
    }

    if (attr == IS_FOOD && m_script_ref != LUA_NOREF) {
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "type");
        if (lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "food") == 0) {
            lua_pop(L, 2);
            return 1;
        }
        lua_pop(L, 2);
    }

    if (attr == CAN_COMBINE && m_script_ref != LUA_NOREF) {
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "can_combine");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2);
            return T::query_attr(attr);
        }
        int result = lua_toboolean(L, -1);
        lua_pop(L, 2);
        return result;
    }
    return T::query_attr(attr);
}

// 重写 CanCombineInBox：从 Lua 脚本属性读取 can_combine_in_box
// 当脚本定义了 can_combine_in_box 属性时，返回其布尔值；否则回退到基类
template <typename T>
int CLuaItemT<T>::can_combine_in_box()
{
    if (m_script_ref != LUA_NOREF) {
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "can_combine_in_box");
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2);
            return T::can_combine_in_box();
        }
        int result = lua_toboolean(L, -1);
        lua_pop(L, 2);
        return result;
    }
    return T::can_combine_in_box();
}

// LuaRoom.inl : 由 Lua 脚本定义的地图/房间模板实现
// 注意：此文件被 LuaRoom.h 包含（模板类），不作为独立编译单元
//
//---------------------------------------------------------

#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaRoom.h"

#include <lua.hpp>
#include <vector>

// Room 实例注册表的非模板实现见 LuaReload.cpp

template <typename T>
static void c_lua_room_t_reload_bridge(void* p)
{
    static_cast<CLuaRoomT<T>*>(p)->reload_script();
}

template <typename T>
CLuaRoomT<T>::CLuaRoomT(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
    CLuaRoomInstanceRegistry::register_entry(this, m_script_name.c_str(),
                                             c_lua_room_t_reload_bridge<T>);
}

template <typename T>
CLuaRoomT<T>::~CLuaRoomT()
{
    CLuaRoomInstanceRegistry::unregister(this);
}

// 调用 Lua 脚本的 create(self)
template <typename T>
void CLuaRoomT<T>::create()
{
    LOG_TRACE("IN");
    do {
        LOG_DEBUG("m_scriptName=%s", m_script_name.c_str());
        // 加载脚本（若已加载且文件修改则自动热重载）
        m_script_ref
            = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_ROOM);
        if (m_script_ref == LUA_NOREF) {
            // 脚本加载失败，回退到基类
            T::create();
            break;
        }

        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "create");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            T::create();
            break;
        }

        // 栈: [table][func]，压入 self (C++ 房间对象)
        lua_bind_push_object(L, this);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            LOG_ERROR("Lua room create failed [%s]: %s", m_script_name.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  // 弹出 table
    } while (0);
    LOG_TRACE("OUT");
}

// 调用 Lua 脚本的 create(self, nFlag)（仅 CFB 系基类，用标签分派回退）
template <typename T>
void CLuaRoomT<T>::create(int flag)
{
    create_int_impl(has_int_create<T>(), flag);
}

template <typename T>
void CLuaRoomT<T>::create_int_impl(std::true_type, int flag)
{
    // 加载脚本（若已加载且文件修改则自动热重载）
    m_script_ref
        = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_ROOM);
    if (m_script_ref == LUA_NOREF) {
        // 脚本加载失败，回退到基类
        T::create(flag);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "create");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::create(flag);
        return;
    }

    // 栈: [table][func]，压入 self (C++ 房间对象) 和 nFlag
    lua_bind_push_object(L, this);
    lua_pushinteger(L, flag);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua fb create failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

template <typename T>
void CLuaRoomT<T>::create_int_impl(std::false_type, int flag)
{
    // 基类无
    // create(int)（CRoom/CFBRoom），此重写不作为虚重写被调用。
    // 直接调用脚本 create(self, nFlag)，失败无基类可回退。
    m_script_ref
        = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_ROOM);
    if (m_script_ref == LUA_NOREF) return;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "create");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    lua_bind_push_object(L, this);
    lua_pushinteger(L, flag);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua fb create failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// reset()：回退语义——脚本定义了 reset(self) 时由脚本完全接管（需要基类
// 行为时脚本显式调用 self:base_reset()）；脚本未定义/未加载时执行 C++ 基类
// reset（NPC 复位/物品清理/房产税金）。
template <typename T>
void CLuaRoomT<T>::reset()
{
    // 刷新脚本引用 + 调 Lua reset()；脚本未定义 reset 时回退基类
    if (!reload_script()) T::reset();
}

// 刷新脚本引用并调用 Lua reset(self) 回调（热重载/wiz update 共用）
// 返回 1 表示脚本定义了 reset 并已调用；0 表示未加载/未定义
template <typename T>
int CLuaRoomT<T>::reload_script()
{
    // 刷新脚本引用（支持热重载：CheckForReload 已更新 m_scripts 中的 ref）
    LuaScript* script
        = g_LuaEngine->get_script(m_script_name.c_str(), LUA_SCRIPT_ROOM);
    if (script && script->ref != LUA_NOREF)
        m_script_ref = script->ref;
    else
        m_script_ref
            = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_ROOM);

    if (m_script_ref == LUA_NOREF) return 0;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "reset");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    lua_bind_push_object(L, this);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua room reset failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
    return 1;
}

// 调用 Lua 脚本的 init(self, me)（若脚本定义了 init）
template <typename T>
void CLuaRoomT<T>::init(CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        T::init(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "init");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::init(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua room init failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 调用 Lua 脚本的 do_look(self, me)（若脚本定义了 do_look）
template <typename T>
int CLuaRoomT<T>::do_look(CChar* me)
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
        LOG_ERROR("Lua room do_look failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::do_look(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);  // 弹出结果和 table
    return result;
}

// 调用 Lua 脚本的 setup(self)
// 回退语义：脚本定义了 setup(self) 时由脚本完全接管（需要基类行为时
// 脚本显式调用 self:base_setup()）；脚本未定义/未加载时执行 C++ 基类
// setup（设置 map/area/city 等关键属性）。
template <typename T>
void CLuaRoomT<T>::setup()
{
    if (m_script_ref == LUA_NOREF) {
        T::setup();
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "setup");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::setup();
        return;
    }

    lua_bind_push_object(L, this);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua room setup failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// CAN_SAVE 读脚本声明的 can_save 字段（构造期即可用，早于 create/load 时序）；
// IS_RESIDENT 读脚本声明的 resident_area 字段；字段缺席时透传基类
template <typename T>
int32_t CLuaRoomT<T>::query_attr(int attr)
{
    if (attr == CAN_SAVE && g_LuaEngine
        && g_LuaEngine->get_script_bool_field(m_script_name.c_str(),
                                              LUA_SCRIPT_ROOM, "can_save")) {
        return 1;
    }
    if (attr == IS_RESIDENT && g_LuaEngine
        && g_LuaEngine->get_script_bool_field(
            m_script_name.c_str(), LUA_SCRIPT_ROOM, "resident_area")) {
        return 1;
    }
    return T::query_attr(attr);
}

// 调用 Lua 脚本的 handle_action(self, comm, me, arg, tar)（若脚本定义了
// handle_action）
template <typename T>
int CLuaRoomT<T>::handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (m_script_ref == LUA_NOREF) return T::handle_action(comm, me, arg, tar);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "handle_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::handle_action(comm, me, arg, tar);
    }

    // 参数顺序：self, comm, me, arg, tar（与 Lua 脚本声明一致）
    // 编码契约：comm/arg/tar 保持服务器内部 GBK 直接压栈。脚本源码在加载期
    // 已统一转 GBK（load_file_as_gbk），Lua 字符串空间是 GBK——投递原始 GBK
    // 才能与脚本字面量直接比较，与 NPC/Item 分发（LuaNpc.inl / LuaItem.inl）
    // 口径一致。历史上此处曾转成 UTF-8，导致房间钩子与字面量反向。
    lua_bind_push_object(L, this);
    lua_pushstring(L, comm ? comm : "");  // comm
    lua_bind_push_object(L, me);          // me
    lua_pushstring(L, arg ? arg : "");    // arg
    lua_pushstring(L, tar ? tar : "");    // tar

    if (lua_pcall(L, 5, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua room handle_action failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::handle_action(comm, me, arg, tar);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);  // 弹出结果和 table
    return result;
}

// 调用 Lua 脚本的 valid_leave(self, me, dir)（若脚本定义了 valid_leave）
template <typename T>
int CLuaRoomT<T>::valid_leave(CChar* me, const char* dir)
{
    return valid_leave_impl(has_valid_leave<T>(), me, dir);
}

template <typename T>
int CLuaRoomT<T>::valid_leave_impl(std::true_type, CChar* me, const char* dir)
{
    if (m_script_ref == LUA_NOREF) return T::valid_leave(me, dir);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_leave");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_leave(me, dir);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    // dir 是服务器内部 GBK 编码（door->querystr("id")），保持原样压栈，
    // 与脚本 GBK 字面量直接比较（编码契约见 handle_action 处说明）
    lua_pushstring(L, dir ? dir : "");

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua room valid_leave failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_leave(me, dir);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);  // 弹出结果和 table
    return result;
}

template <typename T>
int CLuaRoomT<T>::valid_leave_impl(std::false_type, CChar* me, const char* dir)
{
    // 基类无 valid_leave（CFB），若脚本定义了则调用脚本，否则默认允许离开
    if (m_script_ref == LUA_NOREF) return 1;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_leave");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 1;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    // dir 是服务器内部 GBK 编码（door->querystr("id")），保持原样压栈，
    // 与脚本 GBK 字面量直接比较（编码契约见 handle_action 处说明）
    lua_pushstring(L, dir ? dir : "");

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua fb valid_leave failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return 1;
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);  // 弹出结果和 table
    return result;
}

// 调用 Lua 脚本的 desc(self, me)（若脚本定义了 desc）
template <typename T>
void CLuaRoomT<T>::desc(CChar* me)
{
    desc_impl(has_desc<T>(), me);
}

template <typename T>
void CLuaRoomT<T>::desc_impl(std::true_type, CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        T::desc(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "desc");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::desc(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua fb desc failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

template <typename T>
void CLuaRoomT<T>::desc_impl(std::false_type, CChar* me)
{
    // 基类无 desc（房间），若脚本定义了则调用脚本，否则无操作
    if (m_script_ref == LUA_NOREF) return;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "desc");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua room desc failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

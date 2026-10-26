// LuaNpc.inl : 由 Lua 脚本定义的 NPC 模板实现
// 注意：此文件被 LuaNpc.h 包含（模板类），不作为独立编译单元
//
//---------------------------------------------------------

#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaNpc.h"

#include <lua.hpp>
#include <vector>

// NPC 实例注册表的非模板实现见 LuaReload.cpp

// 模板类 reload_script 实现（静态函数指针回调）
template <typename T>
static void c_lua_npc_t_reload_bridge(void* p)
{
    static_cast<CLuaNpcT<T>*>(p)->reload_script();
}

template <typename T>
CLuaNpcT<T>::CLuaNpcT(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
    CLuaNpcInstanceRegistry::register_entry(this, m_script_name.c_str(),
                                            c_lua_npc_t_reload_bridge<T>);
}

template <typename T>
CLuaNpcT<T>::~CLuaNpcT()
{
    CLuaNpcInstanceRegistry::unregister(this);
}

// 刷新脚本引用并重新执行 create()（热重载用）
// 默认行为：purge 背包 + 重新 create()，使脚本改动立即生效
// 脚本可定义 reload(self) 覆盖此行为（仅刷新引用，不重新 create）
template <typename T>
void CLuaNpcT<T>::reload_script()
{
    // 刷新脚本引用
    LuaScript* script
        = g_LuaEngine->get_script(m_script_name.c_str(), LUA_SCRIPT_NPC);
    if (!script || script->ref == LUA_NOREF) return;
    m_script_ref = script->ref;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    // 检查脚本是否定义了 reload()（可选覆盖）
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "reload");
    if (lua_isfunction(L, -1)) {
        // 脚本定义了 reload()：调用它（脚本自行决定如何刷新）
        lua_bind_push_object(L, this);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            LOG_ERROR("Lua npc reload failed [%s]: %s", m_script_name.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  // 弹出 table
        return;
    }
    lua_pop(L, 2);  // 弹出 nil 和 table

    // 默认行为：purge 背包 + 重新 create()
    purge();
    create();
}

// 调用 Lua 脚本的 create 函数
template <typename T>
void CLuaNpcT<T>::create()
{
    LOG_TRACE("CLuaNpcT<T>::create() IN");
    do {
        LOG_DEBUG("m_scriptName=%s", m_script_name.c_str());
        // 加载脚本（若已加载且文件修改则自动热重载）
        m_script_ref
            = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_NPC);
        if (m_script_ref == LUA_NOREF) {
            LOG_ERROR("%s: LoadScript Failed.");
            // 脚本加载失败，回退到基类
            T::create();
            break;
        }

        // 调用脚本的 create(self)，self 是 C++ 对象
        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "create");
        if (!lua_isfunction(L, -1)) {
            LOG_DEBUG("%s: script no create().");
            lua_pop(L, 2);
            T::create();
            break;
        }

        // 栈: [table][func]
        // 压入 self (C++ 对象)
        lua_bind_push_object(L, this);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            LOG_ERROR("Lua npc create failed [%s]: %s", m_script_name.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  // 弹出 table
    } while (0);
    LOG_TRACE("CLuaNpcT<T>::create() OUT");
}

// 调用 Lua 脚本的 setup 函数（C++ 框架在 create() 之后调用 setup()）
// 与 init/handle_action 等方法一致：脚本未定义 setup 时回退到基类，
// 脚本定义了 setup 时由脚本决定是否调用 self:base_setup()。
template <typename T>
void CLuaNpcT<T>::setup()
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
        LOG_ERROR("Lua npc setup failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 处理 Lua 脚本注册的询问逻辑（set_inquiry / set_inquiry_func）
template <typename T>
int CLuaNpcT<T>::do_lua_inquiry(CChar* me, char* ask)
{
    if (!me || !ask) return 0;

    if (m_list_lua_inquiry.find(ask) == m_list_lua_inquiry.end()) {
        if (m_list_lua_inquiry.empty()) {
            return notify_fail("对方并不愿意理你。");
        }
        // 未命中：列出所有询问菜单
        std::map<std::string, LuaInquiry_t>::iterator p;
        for (p = m_list_lua_inquiry.begin(); p != m_list_lua_inquiry.end();
             p++) {
            const LuaInquiry_t& inq = p->second;
            if (!inq.topic.empty()) {
                add_menu_item(inq.topic.c_str(), p->first.c_str(), me);
            }
            else {
                char msg[255];
                snprintf(msg, 255, "向%s请教关于%s的问题。", name(),
                         p->first.c_str());
                add_menu_item(msg, p->first.c_str(), me);
            }
        }
        send_menu(me);
        return 1;
    }

    LuaInquiry_t& inq = m_list_lua_inquiry[ask];
    if (!inq.isFunc) {
        // 预置回答
        say(inq.answer.c_str(), me);
        send_menu(me);
        return 1;
    }

    // 回调：从脚本 table 取函数调用，返回字符串再 say
    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, inq.funcName.c_str());
    if (lua_isfunction(L, -1)) {
        lua_bind_push_object(L, this);
        lua_bind_push_object(L, me);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            LOG_ERROR("Lua inquiry %s failed: %s", inq.funcName.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 2);
        }
        else if (lua_isstring(L, -1)) {
            // lua_pop 会让 lua_tostring 返回的指针失效，先取值再弹栈
            std::string answer = lua_tostring(L, -1);
            lua_pop(L, 2);
            if (!answer.empty()) {
                say(answer.c_str(), me);
                send_menu(me);
            }
        }
        else {
            lua_pop(L, 2);
        }
    }
    else {
        lua_pop(L, 2);
    }
    return 1;
}

// 供 Lua 脚本显式调用的 base_do_talk：与脚本未定义 do_talk 时的回退逻辑一致
template <typename T>
int CLuaNpcT<T>::base_do_talk(CChar* me, char* ask)
{
    if (!me || !ask) return 0;

    // 通用对话钩子：脚本侧（任务等）接管本次对话则终止对话；
    // 钩子判定归引擎流程则走引擎询问菜单；
    // 钩子不适用则商家/钱庄走引擎流程，其余走脚本自己的询问菜单。
    TalkHookResult hook_result = query_talk_hook(this, me, ask);
    if (hook_result.handled) return 1;
    if (hook_result.engine_flow) return T::do_talk_inquiry(me, ask);
    if (dynamic_cast<CVendor*>(this)) return T::do_talk_inquiry(me, ask);
    if (dynamic_cast<CBankOwner*>(this)) return T::do_talk_inquiry(me, ask);
    return do_lua_inquiry(me, ask);
}

// 调用 Lua 脚本的 do_talk 函数
template <typename T>
int CLuaNpcT<T>::do_talk(CChar* me, char* ask)
{
    if (m_script_ref == LUA_NOREF) return T::do_talk(me, ask);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    // 获取脚本 table
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "do_talk");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        // 脚本未定义 do_talk：与脚本显式调用 base_do_talk 的回退逻辑一致
        return base_do_talk(me, ask);
    }

    // 参数：self, me, ask
    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, ask ? ask : "");

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua do_talk failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return base_do_talk(me, ask);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);  // 弹出结果和 table
    // 脚本返回什么就返回什么，与原始 C++ 子类覆写语义一致：
    // 需要回退基类时，脚本应显式调用 self:base_do_talk(me, ask)。
    return result;
}

// 调用 Lua 脚本的 handle_action 函数
template <typename T>
int CLuaNpcT<T>::handle_action(char* comm, CChar* me, char* arg)
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

    // 压参顺序：self, comm, me, arg（与脚本约定一致）
    lua_bind_push_object(L, this);
    lua_pushstring(L, comm ? comm : "");
    lua_bind_push_object(L, me);
    lua_pushstring(L, arg ? arg : "");

    if (lua_pcall(L, 4, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua handle_action failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::handle_action(comm, me, arg);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    // 脚本返回什么就返回什么，与原始 C++ 子类覆写语义一致：
    // 需要回退基类时，脚本应显式调用 self:base_handle_action(comm, me, arg)。
    return result;
}

template <typename T>
int CLuaNpcT<T>::handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (m_script_ref == LUA_NOREF)
        return call_base_handle_action4(npc_has_handle_action4<T>(), comm, me,
                                        arg, tar);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "handle_action4");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return call_base_handle_action4(npc_has_handle_action4<T>(), comm, me,
                                        arg, tar);
    }

    // 压参顺序：self, comm, me, arg, tar（与脚本约定一致）
    lua_bind_push_object(L, this);
    lua_pushstring(L, comm ? comm : "");
    lua_bind_push_object(L, me);
    lua_pushstring(L, arg ? arg : "");
    lua_pushstring(L, tar ? tar : "");

    if (lua_pcall(L, 5, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua handle_action4 failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return call_base_handle_action4(npc_has_handle_action4<T>(), comm, me,
                                        arg, tar);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    // 脚本返回什么就返回什么，与原始 C++ 子类覆写语义一致：
    // 需要回退基类时，脚本应显式调用
    // self:base_handle_action4(comm, me, arg, tar)。
    return result;
}

// 调用 Lua 脚本的 accept_object 函数
template <typename T>
int CLuaNpcT<T>::accept_object(CChar* me, CContainer* item)
{
    if (m_script_ref == LUA_NOREF) return T::accept_object(me, item);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "accept_object");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::accept_object(me, item);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_bind_push_object(L, item);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua accept_object failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::accept_object(me, item);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 init 函数
template <typename T>
void CLuaNpcT<T>::init(CChar* me)
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
        LOG_ERROR("Lua npc init failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 调用 Lua 脚本的 die 函数
template <typename T>
void CLuaNpcT<T>::die()
{
    if (m_script_ref == LUA_NOREF) {
        T::die();
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "die");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::die();
        return;
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc die failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table

    // 脚本 die() 完全控制死亡逻辑，与原始 C++ 子类覆写 die() 语义一致：
    // 需要标准死亡流程时，脚本应显式调用 self:die_base()。
    // 需要自定义死亡（如 destruct）时，脚本自行处理。
}

// 调用 Lua 脚本的 finish_kill 函数（若存在，如巡捕击杀玩家）
template <typename T>
void CLuaNpcT<T>::finish_kill(CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        T::finish_kill(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "finish_kill");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::finish_kill(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc finish_kill failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 调用 Lua 脚本的 is_defeated 函数（若存在，如比武大会登记得胜者）
template <typename T>
void CLuaNpcT<T>::is_defeated(CChar* winner, CChar* failer)
{
    if (m_script_ref == LUA_NOREF) {
        T::is_defeated(winner, failer);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "is_defeated");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::is_defeated(winner, failer);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, winner);
    lua_bind_push_object(L, failer);

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc is_defeated failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 调用 Lua 脚本的 do_look 函数
template <typename T>
int CLuaNpcT<T>::do_look(CChar* me)
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
        LOG_ERROR("Lua npc do_look failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::do_look(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 hit_ob 函数
template <typename T>
const char* CLuaNpcT<T>::hit_ob(CChar* me, CChar* victim, int32_t damage_bonus)
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
        LOG_ERROR("Lua npc hit_ob failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::hit_ob(me, victim, damage_bonus);
    }

    // 返回值可以是字符串（提示信息）或 nil/空字符串（表示无消息）
    if (lua_isstring(L, -1)) {
        // lua_pop 会让 lua_tostring 返回的指针失效，先拷贝再弹栈
        static char buf[256];
        strncpy(buf, lua_tostring(L, -1), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        lua_pop(L, 2);
        if (!buf[0]) return NULL;
        return buf;
    }
    lua_pop(L, 2);
    return NULL;
}

// 调用 Lua 脚本的 greeting 函数
template <typename T>
char* CLuaNpcT<T>::greeting(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::greeting(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "greeting");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::greeting(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua npc greeting failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::greeting(me);
    }

    if (lua_isstring(L, -1)) {
        // lua_pop 会让 lua_tostring 返回的指针失效，先拷贝再弹栈
        static char buf[256];
        strncpy(buf, lua_tostring(L, -1), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        lua_pop(L, 2);
        if (!buf[0]) return NULL;
        return buf;
    }
    lua_pop(L, 2);
    return NULL;
}

// 调用 Lua 脚本的 chat_msg_combat 函数
template <typename T>
char* CLuaNpcT<T>::chat_msg_combat()
{
    if (m_script_ref == LUA_NOREF) return T::chat_msg_combat();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "chat_msg_combat");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::chat_msg_combat();
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua npc chat_msg_combat failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::chat_msg_combat();
    }

    if (lua_isstring(L, -1)) {
        // lua_pop 会让 lua_tostring 返回的指针失效，先拷贝再弹栈
        static char buf[256];
        strncpy(buf, lua_tostring(L, -1), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        lua_pop(L, 2);
        if (!buf[0]) return NULL;
        return buf;
    }
    lua_pop(L, 2);
    return NULL;
}

// 调用 Lua 脚本的 chat_msg 函数
template <typename T>
char* CLuaNpcT<T>::chat_msg()
{
    char* ret = NULL;
    LOG_TRACE("IN");
    do {
        LOG_DEBUG("name=%s", T::querystr("base_name"));
        if (m_script_ref == LUA_NOREF) {
            ret = T::chat_msg();
            break;
        }

        lua_State* L = g_LuaEngine->get_state();
        CLuaActiveRefGuard _guard(m_script_ref);

        lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
        lua_getfield(L, -1, "chat_msg");
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            ret = T::chat_msg();
            break;
        }

        lua_bind_push_object(L, this);

        if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
            LOG_ERROR("Lua npc chat_msg failed [%s]: %s", m_script_name.c_str(),
                      lua_tostring(L, -1));
            lua_pop(L, 2);
            ret = T::chat_msg();
            break;
        }

        if (lua_isstring(L, -1)) {
            // lua_pop 会让 lua_tostring 返回的指针失效，先拷贝再弹栈
            static char buf[256];
            strncpy(buf, lua_tostring(L, -1), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            lua_pop(L, 2);
            if (buf[0]) ret = buf;
            break;
        }
        lua_pop(L, 2);
        ret = NULL;
    } while (0);
    LOG_TRACE("OUT");
    return ret;
}

// 注册预置回答询问：set_inquiry(ask, answer, topic)
template <typename T>
void CLuaNpcT<T>::set_inquiry(const char* ask, const char* answer,
                              const char* topic)
{
    if (!ask || !answer) return;
    LuaInquiry_t inq;
    inq.topic               = topic ? topic : "";
    inq.answer              = answer;
    inq.isFunc              = false;
    m_list_lua_inquiry[ask] = inq;
}

// 注册回调询问：set_inquiry_func(ask, funcName, topic)，funcName 为脚本 table
// 中的函数名
template <typename T>
void CLuaNpcT<T>::set_inquiry_func(const char* ask, const char* funcName,
                                   const char* topic)
{
    if (!ask || !funcName) return;
    LuaInquiry_t inq;
    inq.topic               = topic ? topic : "";
    inq.funcName            = funcName;
    inq.isFunc              = true;
    m_list_lua_inquiry[ask] = inq;
}

// 重写 attempt_apprentice：调用 Lua 脚本的 attempt_apprentice 函数（若存在）
template <typename T>
void CLuaNpcT<T>::attempt_apprentice(CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        T::attempt_apprentice(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "attempt_apprentice");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::attempt_apprentice(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc attempt_apprentice failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 重写 prevent_learn：调用 Lua 脚本的 prevent_learn 函数（若存在）
template <typename T>
int CLuaNpcT<T>::prevent_learn(CChar* me, const char* skill)
{
    if (m_script_ref == LUA_NOREF) return T::prevent_learn(me, skill);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "prevent_learn");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::prevent_learn(me, skill);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, skill ? skill : "");

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua npc prevent_learn failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::prevent_learn(me, skill);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 recognize_apprentice：调用 Lua 脚本的 recognize_apprentice
// 函数（若存在）
template <typename T>
int CLuaNpcT<T>::recognize_apprentice(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::recognize_apprentice(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "recognize_apprentice");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::recognize_apprentice(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua npc recognize_apprentice failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::recognize_apprentice(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 accept_fight：调用 Lua 脚本的 accept_fight 函数（若存在）
template <typename T>
int CLuaNpcT<T>::accept_fight(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::accept_fight(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "accept_fight");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::accept_fight(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua npc accept_fight failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::accept_fight(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 调用 Lua 脚本的 attack 函数（若存在）
template <typename T>
void CLuaNpcT<T>::attack(CChar* opponent)
{
    if (m_script_ref == LUA_NOREF) {
        T::attack(opponent);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "attack");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::attack(opponent);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, opponent);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc attack failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 调用 Lua 脚本的 relay_say（若存在）
template <typename T>
void CLuaNpcT<T>::relay_say(CChar* me, char* msg)
{
    if (m_script_ref == LUA_NOREF) {
        T::relay_say(me, msg);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "relay_say");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::relay_say(me, msg);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, msg ? msg : "");

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc relay_say failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// 调用 Lua 脚本的 relay_emote（若存在）
template <typename T>
void CLuaNpcT<T>::relay_emote(CChar* me, std::string verb)
{
    if (m_script_ref == LUA_NOREF) {
        T::relay_emote(me, verb);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "relay_emote");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::relay_emote(me, verb);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_pushstring(L, verb.c_str());

    if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua npc relay_emote failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

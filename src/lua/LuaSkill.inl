// LuaSkill.inl : 由 Lua 脚本定义的 Skill 模板实现
// 注意：此文件被 LuaSkill.h 包含（模板类），不作为独立编译单元
//
////////////////////////////////////////////////////////////////////

#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaSkill.h"

#include <lua.hpp>
#include "Global.h"

// 清除 no_cast 状态（法术冷却）
static void lua_remove_no_cast(CContainer* ob, int32_t param1, int32_t param2)
{
    ob->delete_temp("no_cast");
}

template <typename T>
CLuaSkillT<T>::CLuaSkillT(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
}

template <typename T>
CLuaSkillT<T>::~CLuaSkillT()
{
}

// 调用 Lua 脚本的 create 函数
template <typename T>
void CLuaSkillT<T>::create()
{
    LOG_DEBUG("m_scriptName=%s", m_script_name.c_str());
    // 加载脚本（若已加载且文件修改则自动热重载）
    m_script_ref
        = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_SKILL);
    if (m_script_ref == LUA_NOREF) {
        // 脚本加载失败，回退到基类
        // 留痕：证明本次 create 走了"回退到 C++ 基类"路径
        LOG_ERROR(
            "Lua skill create fallback [%s]: script load failed, using base "
            "class",
            m_script_name.c_str());
        T::create();
        return;
    }

    // 设置 base_skill（可 enable 的技能类型）。
    // 此时脚本已加载（m_scriptRef 有效），valid_enable 可正常回调脚本逻辑。
    setup_base_skill();

    // 调用脚本的 create(self)，self 是 C++ 对象
    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "create");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        // 脚本未定义 create：回退 C++ 基类
        // （CForce::create 注册复元术、CThrowing::create 注册偷袭）。
        // 脚本定义了 create 时由脚本完全接管，需要基类副作用时
        // 脚本应显式调用 self:base_create()。
        T::create();
        return;
    }

    // 栈: [table][func]
    lua_bind_push_object(L, this);
    // 栈: [table][func][self]
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua skill create failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

// 设置 base_skill（可 enable 的技能类型）
template <typename T>
void CLuaSkillT<T>::setup_base_skill()
{
    char valid_types[22][10] = {
        "music", "axe",    "leg",      "unarmed", "sword", "blade",
        "staff", "club",   "throwing", "force",   "parry", "lingfa",
        "dodge", "whip",   "magic",    "finger",  "hand",  "cuff",
        "claw",  "strike", "hammer",   "dagger",
    };

    string msg = "";
    for (int i = 0; i < 22; i++) {
        if (valid_enable(valid_types[i])) {
            msg += valid_types[i];
            msg += ",";
        }
    }
    set("base_skill", msg.c_str());
}

// 调用 Lua 脚本的 query_action 函数
template <typename T>
CMapping* CLuaSkillT<T>::query_action(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::query_action(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::query_action(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_action failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::query_action(me);
    }

    // 脚本返回一个 table（action/dodge/parry/force/damage），转成 CMapping
    static CMapping m_actions;
    m_actions.clear();

    if (lua_istable(L, -1)) {
        // 读取 action
        lua_getfield(L, -1, "action");
        if (lua_isstring(L, -1)) {
            m_actions.set("action", lua_tostring(L, -1));
        }
        lua_pop(L, 1);

        // 读取 dodge
        lua_getfield(L, -1, "dodge");
        if (lua_isnumber(L, -1))
            m_actions.set("dodge", (int32_t)lua_tointeger(L, -1));
        lua_pop(L, 1);

        // 读取 parry
        lua_getfield(L, -1, "parry");
        if (lua_isnumber(L, -1))
            m_actions.set("parry", (int32_t)lua_tointeger(L, -1));
        lua_pop(L, 1);

        // 读取 force
        lua_getfield(L, -1, "force");
        if (lua_isnumber(L, -1))
            m_actions.set("force", (int32_t)lua_tointeger(L, -1));
        lua_pop(L, 1);

        // 读取 damage
        lua_getfield(L, -1, "damage");
        if (lua_isnumber(L, -1))
            m_actions.set("damage", (int32_t)lua_tointeger(L, -1));
        lua_pop(L, 1);

        // 读取 lvl
        lua_getfield(L, -1, "lvl");
        if (lua_isnumber(L, -1))
            m_actions.set("lvl", (int32_t)lua_tointeger(L, -1));
        lua_pop(L, 1);

        // 读取 damage_type
        lua_getfield(L, -1, "damage_type");
        if (lua_isstring(L, -1))
            m_actions.set("damage_type", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pop(L, 2);  // 弹出结果和 table
    return &m_actions;
}

// 调用 Lua 脚本的 valid_learn 函数
template <typename T>
int CLuaSkillT<T>::valid_learn(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::valid_learn(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_learn");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_learn(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_learn failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_learn(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 注册 Lua 法术（供 Lua 脚本调用）
template <typename T>
int CLuaSkillT<T>::lua_add_magic(const char* magicName, const char* displayName,
                                 int32_t mp, int32_t ap, int target,
                                 int validCast, const char* luaFuncName,
                                 int hostile)
{
    if (!magicName || !luaFuncName) return 0;

    // 创建 Magic_t 并加入 m_MagicList（用于 DownLoad 显示）
    auto magic              = std::make_unique<Magic_t>();
    magic->name             = displayName ? displayName : magicName;
    magic->mp               = mp;
    magic->ap               = ap;
    magic->target           = target;
    magic->valid_cast       = validCast;
    magic->hostile          = hostile;
    magic->func             = NULL;  // 法术函数由 Cast 重写处理
    m_magic_list[magicName] = std::move(magic);

    // 记录 Lua 法术名 -> Lua 函数名
    m_lua_magic_list[magicName] = luaFuncName;

    return 1;
}

// 获取已注册的 Lua 法术名
template <typename T>
const char* CLuaSkillT<T>::get_magic_name(int idx)
{
    int i = 0;
    std::map<std::string, std::string>::iterator it;
    for (it = m_lua_magic_list.begin(); it != m_lua_magic_list.end();
         it++, i++) {
        if (i == idx) return it->first.c_str();
    }
    return "";
}

// 获取法术的显示名（用于验证编码）
template <typename T>
const char* CLuaSkillT<T>::get_magic_display_name(int idx)
{
    int i = 0;
    CastList_t::iterator it;
    for (it = m_magic_list.begin(); it != m_magic_list.end(); it++, i++) {
        if (i == idx) return it->second->name.c_str();
    }
    return "";
}

// 重写 Cast：支持 Lua 法术
template <typename T>
int CLuaSkillT<T>::cast(string magic, CChar* me, CContainer* target)
{
    // 检查是否是 Lua 法术
    std::map<std::string, std::string>::iterator it
        = m_lua_magic_list.find(magic);
    if (it == m_lua_magic_list.end())
        return T::cast(magic, me, target);  // 回退基类，支持 C++ 注册法术

    // 调用 Lua 法术函数
    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, it->second.c_str());
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_bind_push_object(L, target);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill cast %s failed [%s]: %s", magic.c_str(),
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return 0;
    }

    int ret = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);

    if (ret) {
        // 法术成功，更新状态
        me->set_temp("no_cast", 1);
        me->call_out(lua_remove_no_cast, ret);
        me->send_state();
    }
    return ret;
}

// 重写 valid_enable
template <typename T>
int CLuaSkillT<T>::valid_enable(string usage)
{
    if (m_script_ref == LUA_NOREF) return T::valid_enable(usage);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_enable");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_enable(usage);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, usage.c_str());

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_enable failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_enable(usage);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 practice_skill
template <typename T>
int CLuaSkillT<T>::practice_skill(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::practice_skill(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "practice_skill");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::practice_skill(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill practice_skill failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::practice_skill(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_force
template <typename T>
int CLuaSkillT<T>::valid_force(const char* arg)
{
    if (m_script_ref == LUA_NOREF) return T::valid_force(arg);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_force");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_force(arg);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, arg ? arg : "");

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_force failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_force(arg);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_force_pro
template <typename T>
int CLuaSkillT<T>::valid_force_pro(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::valid_force_pro(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_force_pro");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_force_pro(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_force_pro failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_force_pro(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_combine
template <typename T>
int CLuaSkillT<T>::valid_combine(string combo)
{
    if (m_script_ref == LUA_NOREF) return T::valid_combine(combo);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_combine");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::valid_combine(combo);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, combo.c_str());

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_combine failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::valid_combine(combo);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 query_dodge_msg
template <typename T>
const char* CLuaSkillT<T>::query_dodge_msg(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return T::query_dodge_msg(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_dodge_msg");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::query_dodge_msg(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_dodge_msg failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::query_dodge_msg(me);
    }

    static char buf[512];
    if (lua_isstring(L, -1)) {
        const char* msg = lua_tostring(L, -1);
        if (msg && msg[0]) {
            strncpy(buf, msg, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            lua_pop(L, 2);
            return buf;
        }
    }
    lua_pop(L, 2);
    buf[0] = 0;
    return buf;
}

// 重写 query_parry_msg
template <typename T>
const char* CLuaSkillT<T>::query_parry_msg(CContainer* weapon)
{
    if (m_script_ref == LUA_NOREF) return T::query_parry_msg(weapon);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_parry_msg");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::query_parry_msg(weapon);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, weapon);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_parry_msg failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::query_parry_msg(weapon);
    }

    // 存入成员缓存，保证返回指针在下次调用前有效
    const char* msg = lua_tostring(L, -1);
    if (msg)
        m_parry_msg_cache = msg;
    else
        m_parry_msg_cache.clear();
    lua_pop(L, 2);
    return m_parry_msg_cache.c_str();
}

// 重写 hit_ob
template <typename T>
char* CLuaSkillT<T>::hit_ob(CChar* me, CChar* victim, int32_t& damage)
{
    if (m_script_ref == LUA_NOREF) return "";

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "hit_ob");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return "";
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);
    lua_bind_push_object(L, victim);
    lua_pushinteger(L, damage);

    // 期望两个返回值：damage（数值）、msg（命中消息，可选）
    if (lua_pcall(L, 4, 2, 0) != LUA_OK) {
        LOG_ERROR("Lua skill hit_ob failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return "";
    }

    // 读取返回值（可能更新 damage）
    if (lua_isnumber(L, -2)) damage = (int32_t)lua_tointeger(L, -2);

    // 第二返回值：命中消息，存入成员缓存，保证返回指针在下次调用前有效
    m_hit_msg_cache.clear();
    if (lua_isstring(L, -1)) m_hit_msg_cache = lua_tostring(L, -1);
    lua_pop(L, 3);  // 弹出 msg、damage、table
    if (!m_hit_msg_cache.empty()) return (char*)m_hit_msg_cache.c_str();
    return "";
}

// 重写 type
template <typename T>
const char* CLuaSkillT<T>::type()
{
    if (m_script_ref == LUA_NOREF) return T::type();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "type");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return T::type();
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill type failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return T::type();
    }

    // 存入成员缓存，保证返回指针在下次调用前有效
    const char* result = lua_tostring(L, -1);
    if (result)
        m_type_cache = result;
    else
        m_type_cache = T::type();
    lua_pop(L, 2);
    return m_type_cache.c_str();
}

// 重写 skill_improved
template <typename T>
void CLuaSkillT<T>::skill_improved(CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        T::skill_improved(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "skill_improved");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        T::skill_improved(me);
        return;
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        LOG_ERROR("Lua skill skill_improved failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出 table
}

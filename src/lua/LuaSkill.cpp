/**
 * @file LuaSkill.cpp
 * @brief implementation of the CLuaSkill class.
 *
 * 技能行为全部由 Lua 脚本实现；本文件只做「脚本 → 引擎」的回调分派，
 * 以及脚本没实现时的回落（基础技能 / C++ 基类默认实现）。
 */

////////////////////////////////////////////////////////////////////

#include "LuaBind.h"
#include "LuaEngine.h"
#include "LuaSkill.h"

#include <lua.hpp>
#include <set>
#include <string>
#include "CmdHandle.h"  // check_ap / resume_ap：施法的行动点闸门
#include "Global.h"

// 清除 no_cast 状态（法术冷却）
static void lua_remove_no_cast(CContainer* ob, int32_t param1, int32_t param2)
{
    ob->delete_temp("no_cast");
}

CLuaSkill::CLuaSkill(const char* scriptName)
{
    m_script_name = scriptName ? scriptName : "";
    m_script_ref  = LUA_NOREF;
}

CLuaSkill::~CLuaSkill() {}

// 技能创建期保护：防止 A 继承 B、B 又继承 A 造成的无限递归
static std::set<std::string> s_creating_skills;

// 调用 Lua 脚本的 create 函数
void CLuaSkill::create()
{
    LOG_DEBUG("m_scriptName=%s", m_script_name.c_str());
    // 加载脚本（若已加载且文件修改则自动热重载）
    m_script_ref
        = g_LuaEngine->load_script(m_script_name.c_str(), LUA_SCRIPT_SKILL);
    if (m_script_ref == LUA_NOREF) {
        // 脚本加载失败：技能名回落为脚本名（数据键），便于定位是哪个脚本坏了
        LOG_ERROR("Lua skill create failed [%s]: script load failed",
                  m_script_name.c_str());
        set_name(m_script_name.c_str());
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
        // 脚本未定义 create：技能名回落为脚本名（数据键），
        // 便于定位是哪个脚本缺 create
        set_name(m_script_name.c_str());
        inherit_base_magics();
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

    inherit_base_magics();
}

// 设置 base_skill（可 enable 的技能类型）
void CLuaSkill::setup_base_skill()
{
    char valid_types[22][10] = {
        "music", "axe",    "leg",      "unarmed", "sword", "blade",
        "staff", "club",   "throwing", "force",   "parry", "lingfa",
        "dodge", "whip",   "magic",    "finger",  "hand",  "cuff",
        "claw",  "strike", "hammer",   "dagger",
    };

    m_base_usages.clear();
    string msg = "";
    for (int i = 0; i < 22; i++) {
        if (valid_enable(valid_types[i])) {
            msg += valid_types[i];
            msg += ",";
            m_base_usages.push_back(valid_types[i]);
        }
    }
    set("base_skill", msg.c_str());
}

// 继承"可 enable 的技能种类"对应基础技能的法术
// 高级技能 enable 到某技能种类后，基础技能自带的法术（如基本投掷的偷袭）
// 仍要可用：把基础技能的法术登记进本技能，执行时转发回基础技能。
void CLuaSkill::inherit_base_magics()
{
    if (m_base_usages.empty()) return;

    // 已在创建链上的技能名不再继承（防 A→B→A 循环）
    bool owner = s_creating_skills.insert(m_script_name).second;
    for (const std::string& usage : m_base_usages) {
        // 自己不继承自己；创建链上已存在的技能也不再继承（防循环）
        if (usage == m_script_name) continue;
        if (s_creating_skills.count(usage)) continue;
        if (!g_LuaEngine->script_exists(usage.c_str(), LUA_SCRIPT_SKILL))
            continue;

        CSkill* basic = load_skill(usage.c_str());
        if (basic) inherit_magics(basic);
    }
    if (owner) s_creating_skills.erase(m_script_name);
}

// 调用 Lua 脚本的 query_action 函数（若存在），
// 否则回落到基础技能（同族默认实现，如基本投掷的出招姿势）
CMapping* CLuaSkill::query_action(CChar* me)
{
    // 脚本加载失败：与其他回调一致，回落基础技能
    if (m_script_ref == LUA_NOREF) return base_query_action(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_action");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return base_query_action(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_action failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return base_query_action(me);
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

// 脚本未实现 query_action 时，回落到基础技能的同名实现
// 高级投掷若自己没有出招姿势 → 用基本投掷的；
// 与法术继承、hit_ob 回落同构：高级技能顶替基础技能时，
// 基础技能的能力仍然在。
// 只回落到"脚本里确实实现了 query_action"的基础技能，
// 避免继承链成环时递归不止。
CMapping* CLuaSkill::base_query_action(CChar* me)
{
    for (const std::string& usage : m_base_usages) {
        if (usage == m_script_name) continue;
        CSkill* basic = load_skill(usage.c_str());
        if (!basic || basic == this) continue;
        if (!basic->has_query_action()) continue;
        return basic->query_action(me);
    }
    return CSkill::query_action(me);
}

// 本技能是否提供 query_action 实现（脚本里有同名函数）
bool CLuaSkill::has_query_action()
{
    if (m_script_ref == LUA_NOREF) return false;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_action");
    bool found = lua_isfunction(L, -1);
    lua_pop(L, 2);
    return found;
}

// 调用 Lua 脚本的 valid_learn 函数
int CLuaSkill::valid_learn(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return CSkill::valid_learn(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);

    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_learn");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::valid_learn(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_learn failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::valid_learn(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 注册 Lua 法术（供 Lua 脚本调用）
int CLuaSkill::lua_add_magic(const char* magicName, const char* displayName,
                             int32_t mp, int32_t ap, int target, int validCast,
                             const char* luaFuncName, int hostile)
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
    magic->lua_func         = luaFuncName;  // Lua 侧法术函数名
    magic->owner            = this;
    m_magic_list[magicName] = std::move(magic);

    return 1;
}

// 获取已注册法术名
const char* CLuaSkill::get_magic_name(int idx)
{
    int i = 0;
    CastList_t::iterator it;
    for (it = m_magic_list.begin(); it != m_magic_list.end(); it++, i++) {
        if (i == idx) return it->first.c_str();
    }
    return "";
}

// 获取法术的显示名（用于验证编码）
const char* CLuaSkill::get_magic_display_name(int idx)
{
    int i = 0;
    CastList_t::iterator it;
    for (it = m_magic_list.begin(); it != m_magic_list.end(); it++, i++) {
        if (i == idx) return it->second->name.c_str();
    }
    return "";
}

// 重写 Cast：支持 Lua 法术
int CLuaSkill::cast(string magic, CChar* me, CContainer* target)
{
    const Magic_t* entry = find_magic(magic.c_str());
    if (!entry) return 0;  // 法术未注册

    // 继承自基础技能的法术：转发给归属技能执行（逻辑在基础技能脚本里）
    if (entry->owner && entry->owner != this)
        return CSkill::cast(magic, me, target);

    // 无 Lua 实现的法术：基类不提供 C++ 法术实现
    if (entry->lua_func.empty()) return 0;

    // 施法内功前置闸门（已准备内功 / 脚本扩展 / 引擎配置 / 数据集规则）
    if (!check_cast_force(me)) return 0;

    // AP 前置：施法消耗行动点，耗费随技能等级与角色等级递减
    int32_t ap = entry->ap
                 * (120 - me->query_skill(querystr("base_name"), 1) / 4
                    - me->query("level") / 2)
                 / 100;
    if (ap < 0) ap = 1;
    if (!check_ap(ap, me)) return 0;

    // 目标可能在施法中被销毁（如偷袭致死），事后不得再触碰它
    int32_t object_id = target ? target->object_id() : 0;

    // 调用 Lua 法术函数
    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, entry->lua_func.c_str());
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
        if (object_id == g_last_destruct_object) target = NULL;
        // 战斗中扣减行动点
        if (me->is_fighting()) resume_ap(ap, me);
        // 受术者状态刷新：对敌法术可能改了目标的 hp/mp，须通知其客户端
        if (target && entry->target == CAST_TO_OTHER) {
            CChar* victim = dynamic_cast<CChar*>(target);
            if (victim) victim->send_state();
        }
        me->send_state();
    }
    return ret;
}

// 重写 valid_enable
int CLuaSkill::valid_enable(string usage)
{
    if (m_script_ref == LUA_NOREF) return CSkill::valid_enable(usage);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_enable");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::valid_enable(usage);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, usage.c_str());

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_enable failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::valid_enable(usage);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 practice_skill
int CLuaSkill::practice_skill(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return CSkill::practice_skill(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "practice_skill");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::practice_skill(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill practice_skill failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::practice_skill(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_force
int CLuaSkill::valid_force(const char* arg)
{
    if (m_script_ref == LUA_NOREF) return CSkill::valid_force(arg);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_force");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::valid_force(arg);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, arg ? arg : "");

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_force failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::valid_force(arg);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_force_pro
int CLuaSkill::valid_force_pro(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return CSkill::valid_force_pro(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_force_pro");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::valid_force_pro(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_force_pro failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::valid_force_pro(me);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 valid_combine
int CLuaSkill::valid_combine(string combo)
{
    if (m_script_ref == LUA_NOREF) return CSkill::valid_combine(combo);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "valid_combine");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::valid_combine(combo);
    }

    lua_bind_push_object(L, this);
    lua_pushstring(L, combo.c_str());

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill valid_combine failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::valid_combine(combo);
    }

    int result = (int)lua_tointeger(L, -1);
    lua_pop(L, 2);
    return result;
}

// 重写 query_dodge_msg
const char* CLuaSkill::query_dodge_msg(CChar* me)
{
    if (m_script_ref == LUA_NOREF) return CSkill::query_dodge_msg(me);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_dodge_msg");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::query_dodge_msg(me);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, me);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_dodge_msg failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::query_dodge_msg(me);
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
const char* CLuaSkill::query_parry_msg(CContainer* weapon)
{
    if (m_script_ref == LUA_NOREF) return CSkill::query_parry_msg(weapon);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "query_parry_msg");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::query_parry_msg(weapon);
    }

    lua_bind_push_object(L, this);
    lua_bind_push_object(L, weapon);

    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill query_parry_msg failed [%s]: %s",
                  m_script_name.c_str(), lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::query_parry_msg(weapon);
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

// 重写 hit_ob：调用 Lua 脚本的 hit_ob 函数（若存在），
// 否则回落到基础技能（同族默认实现，如基本内功的内功反震）
char* CLuaSkill::hit_ob(CChar* me, CChar* victim, int32_t& damage)
{
    // 脚本加载失败：与其他回调一致，回落基础技能（同族默认实现）
    if (m_script_ref == LUA_NOREF) return base_hit_ob(me, victim, damage);

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "hit_ob");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return base_hit_ob(me, victim, damage);
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

// 脚本未实现 hit_ob 时，回落到基础技能的同名实现
// 高级内功（force_adv）自己没有 hit_ob → 用基本内功的内功反震；
// 与法术继承同构：高级技能顶替基础技能时，基础技能的能力仍然在。
// 只回落到"脚本里确实实现了 hit_ob"的基础技能，避免继承链成环时递归不止。
char* CLuaSkill::base_hit_ob(CChar* me, CChar* victim, int32_t& damage)
{
    for (const std::string& usage : m_base_usages) {
        if (usage == m_script_name) continue;
        CSkill* basic = load_skill(usage.c_str());
        if (!basic || basic == this) continue;
        if (!basic->has_hit_ob()) continue;
        return basic->hit_ob(me, victim, damage);
    }
    return "";
}

// 本技能是否提供 hit_ob 实现（脚本里有同名函数）
bool CLuaSkill::has_hit_ob()
{
    if (m_script_ref == LUA_NOREF) return false;

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "hit_ob");
    bool found = lua_isfunction(L, -1);
    lua_pop(L, 2);
    return found;
}

// 重写 type
const char* CLuaSkill::type()
{
    if (m_script_ref == LUA_NOREF) return CSkill::type();

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "type");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return CSkill::type();
    }

    lua_bind_push_object(L, this);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG_ERROR("Lua skill type failed [%s]: %s", m_script_name.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 2);
        return CSkill::type();
    }

    // 存入成员缓存，保证返回指针在下次调用前有效
    const char* result = lua_tostring(L, -1);
    if (result)
        m_type_cache = result;
    else
        m_type_cache = CSkill::type();
    lua_pop(L, 2);
    return m_type_cache.c_str();
}

// 重写 skill_improved
void CLuaSkill::skill_improved(CChar* me)
{
    if (m_script_ref == LUA_NOREF) {
        CSkill::skill_improved(me);
        return;
    }

    lua_State* L = g_LuaEngine->get_state();
    CLuaActiveRefGuard _guard(m_script_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, m_script_ref);
    lua_getfield(L, -1, "skill_improved");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        CSkill::skill_improved(me);
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

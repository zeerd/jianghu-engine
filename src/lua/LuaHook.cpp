/**
 * @file LuaHook.cpp
 * @brief C++ 主动调取 Lua 的功能增强/补充机制实现。
 */

#include <windows.h>

#include "LuaHook.h"

#include <lua.hpp>

#include <algorithm>
#include <sys/stat.h>

#include "Char.h"
#include "Global.h"
#include "IniParser.h"
#include "LuaBind.h"
#include "LuaEngine.h"

//---------------------------------------------------------
// 全局表
//---------------------------------------------------------

static void hook_ensure_table(lua_State* L, const char* name)
{
    lua_getglobal(L, name);
    if (lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_setglobal(L, name);
}

//---------------------------------------------------------
// 命令模块懒加载分发器
//
// 约定：一个命令一个文件，command/<命令名>.lua 返回模块表，
// 入口函数键名与文件名相同。目录本身就是命令清单，新增命令
// 不需要改动任何共享文件或 C++ 注册表。
//
// commands 表本体仍由脚本直接定义命令（rawset）：分发器只负责
// 表内未命中时的模块解析，脚本定义优先于模块文件。
//---------------------------------------------------------

// 常驻 registry 引用：commands 表本体。命令模块里的 call_out
// 作用在无脚本表的对象（玩家）上时，l_call_out 回退取用它。
static int s_commands_ref = LUA_NOREF;

// 分发器实现放在 Lua 侧（与 CLuaEngine 安装 GBK require searcher 同一惯用法）：
// 需要遍历 package.path 探测文件，io.open / gmatch 在 Lua 里直写最清楚。
//
// 探测先行而非依赖 require 的失败消息分类：require 会把所有 searchers 的
// "no file" 拼进同一条消息，模块文件的语法错误也混在其中，字符串分类
// 无法区分“命令不存在”与“命令脚本写坏了”。
static const char* kCommandsLoaderLua
    = "-- commands 表未命中的命令名 → require(\"command.<命令名>\") "
      "取同名入口函数\n"
      "local cache = {}   -- 已解析命令：命令名 → 入口函数\n"
      "local failed = {}  -- "
      "已尝试且落空的命令名（文件缺席/写坏），避免重复探测与刷日志\n"
      "\n"
      "local function find_file(modpath)\n"
      "    for pattern in package.path:gmatch('[^;]+') do\n"
      "        local p = pattern:gsub('%?', modpath)\n"
      "        local f = io.open(p, 'rb')\n"
      "        if f then\n"
      "            f:close()\n"
      "            return p\n"
      "        end\n"
      "    end\n"
      "    return nil\n"
      "end\n"
      "\n"
      "local function resolve(name)\n"
      "    -- 命令名来自玩家输入：只允许字母/数字/下划线，'.' '/' "
      "会拼出越界模块路径\n"
      "    if not name:match('^[%w_]+$') then\n"
      "        return nil\n"
      "    end\n"
      "    -- "
      "先记失败，成功路径末尾撤销：emote/未知命令与命令名共用此未命中路径，\n"
      "    -- 缺席必须缓存，否则每次表情都重扫 package.path 做磁盘探测\n"
      "    failed[name] = true\n"
      "    local modname = 'command.' .. name\n"
      "    local file = find_file(modname:gsub('%.', '/'))\n"
      "    if not file then\n"
      "        -- 命令不存在属常态（拼错、纯 emote）：静默回落，不记日志\n"
      "        return nil\n"
      "    end\n"
      "    local ok, mod = pcall(require, modname)\n"
      "    if not ok then\n"
      "        log_error('[LuaCommand] %s load failed: %s', modname, mod)\n"
      "        return nil\n"
      "    end\n"
      "    if type(mod) ~= 'table' then\n"
      "        log_error('[LuaCommand] %s must return a module table, got "
      "%s',\n"
      "                  modname, type(mod))\n"
      "        return nil\n"
      "    end\n"
      "    local fn = mod[name]\n"
      "    if type(fn) ~= 'function' then\n"
      "        log_error('[LuaCommand] %s has no entry function %s (file "
      "%s)',\n"
      "                  modname, name, file)\n"
      "        return nil\n"
      "    end\n"
      "    failed[name] = nil\n"
      "    cache[name] = fn\n"
      "    return fn\n"
      "end\n"
      "\n"
      "setmetatable(commands, {\n"
      "    -- 表内已有的命令（脚本直定义）优先于模块文件：__index "
      "只在未命中时触发\n"
      "    __index = function(_, name)\n"
      "        if type(name) ~= 'string' then return nil end\n"
      "        local fn = cache[name]\n"
      "        if fn then return fn end\n"
      "        if failed[name] then return nil end\n"
      "        return resolve(name)\n"
      "    end\n"
      "})\n";

void lua_commands_install(lua_State* L)
{
    if (!L) return;

    if (lua_getglobal(L, "commands") != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "commands");
    }
    else {
        lua_pop(L, 1);
    }

    // 分发器的已解析缓存与失败记录是 chunk 局部变量（闭包 upvalue），
    // 重装即整体作废；commands 表本体中脚本直定义的命令不受影响
    if (luaL_dostring(L, kCommandsLoaderLua) != LUA_OK) {
        LOG_ERROR("[LuaCommand] install dispatcher failed: %s",
                  lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void lua_commands_flush(lua_State* L) { lua_commands_install(L); }

void lua_hook_init_tables(lua_State* L)
{
    if (!L) return;
    hook_ensure_table(L, "hooks");
    hook_ensure_table(L, "commands");
    lua_commands_install(L);
}

//---------------------------------------------------------
// 开机加载
//---------------------------------------------------------

// 读取单个脚本文件：不存在返回 false（尝试下一个根）；
// 存在但加载/执行失败记 LOG_ERROR 并返回 false。
static bool hook_run_script(lua_State* L, const char* root, const char* name)
{
    if (!root || !root[0]) return false;

    std::string path = std::string(root) + "/data/" + name + ".lua";
    std::replace(path.begin(), path.end(), '\\', '/');

    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;

    std::string gbk = load_file_as_gbk(path.c_str());
    if (gbk.empty()) {
        LOG_ERROR("[LuaHook] read %s failed", path.c_str());
        return false;
    }
    if (luaL_loadbuffer(L, gbk.c_str(), gbk.size(), path.c_str()) != LUA_OK) {
        LOG_ERROR("[LuaHook] load %s failed: %s", path.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        LOG_ERROR("[LuaHook] run %s failed: %s", path.c_str(),
                  lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool lua_hook_boot(lua_State* L)
{
    if (!L) return false;

    lua_newtable(L);
    lua_setglobal(L, "hooks");
    lua_newtable(L);
    lua_setglobal(L, "commands");

    const char* roots[2] = {ini_get("lua", "script_root", ""),
                            ini_get("lua", "default_script_root", "")};
    bool all_loaded      = true;

    // data/constants.lua：数据集图片号常量，script_root 优先、回退 default；
    // 缺席静默跳过（icon 非必须属性，常量缺失时脚本不写 icon 即可）
    if (!hook_run_script(L, roots[0], "constants"))
        hook_run_script(L, roots[1], "constants");

    // data/hooks.lua：C++ 挂点的脚本侧实现，缺失/失败记错并置 all_loaded=false
    // （开机校验点）。commands 不在此加载：命令一律 command/<命令名>.lua 按需
    // require，commands 表与分发器完全由 C++ 构造，不依赖任何脚本文件。
    bool hooks_loaded = false;
    for (int r = 0; r < 2 && !hooks_loaded; r++)
        hooks_loaded = hook_run_script(L, roots[r], "hooks");
    if (!hooks_loaded) {
        LOG_ERROR(
            "[LuaHook] data/hooks.lua missing or failed in all script roots");
        all_loaded = false;
    }

    // 命令模块分发器：boot 重建了 commands 表，元表与缓存随之重装
    lua_commands_install(L);

    // commands 表常驻引用：命令模块对玩家等无脚本表对象挂 call_out 时，
    // l_call_out 回退取用（见 lua_call_command）
    if (s_commands_ref != LUA_NOREF)
        luaL_unref(L, LUA_REGISTRYINDEX, s_commands_ref);
    lua_getglobal(L, "commands");
    s_commands_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    return all_loaded;
}

//---------------------------------------------------------
// Lua 命令分发
//---------------------------------------------------------

int lua_call_command(const char* cmd, CChar* me, int32_t target, char* arg)
{
    if (!cmd || !cmd[0]) return -1;

    lua_State* L = g_LuaEngine ? g_LuaEngine->get_state() : nullptr;
    if (!L) return -1;

    int top = lua_gettop(L);
    if (lua_getglobal(L, "commands") != LUA_TTABLE) {
        lua_settop(L, top);
        return -1;
    }
    lua_getfield(L, -1, cmd);
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return -1;
    }
    lua_remove(L, -2);  // 只留函数

    if (me)
        lua_bind_push_object(L, me);
    else
        lua_pushnil(L);
    lua_pushinteger(L, target);
    if (arg)
        lua_pushstring(L, arg);
    else
        lua_pushnil(L);

    // 命令模块内部可能对玩家等无脚本表的对象挂 call_out：l_call_out
    // 反查不到 self 的脚本表时回退取当前脚本引用，此处即 commands 表本体。
    // 引用缺席（未 boot）时 guard 自动跳过，不动既有 active ref。
    CLuaActiveRefGuard active_guard(s_commands_ref);
    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        LOG_ERROR("[LuaHook] command %s failed: %s", cmd, lua_tostring(L, -1));
        lua_settop(L, top);
        return -1;
    }

    int ret = 1;
    if (lua_isnumber(L, -1)) ret = (int)lua_tointeger(L, -1);
    lua_settop(L, top);
    return ret;
}

//---------------------------------------------------------
// CLuaHookCall
//---------------------------------------------------------

CLuaHookCall::CLuaHookCall(const char* name)
    : m_L(nullptr), m_top0(0), m_valid(false), m_called(false)
{
    m_L = g_LuaEngine ? g_LuaEngine->get_state() : nullptr;
    if (!m_L || !name || !name[0]) return;

    m_top0 = lua_gettop(m_L);

    if (lua_getglobal(m_L, "hooks") != LUA_TTABLE) {
        lua_settop(m_L, m_top0);
        return;
    }
    lua_getfield(m_L, -1, name);
    if (!lua_isfunction(m_L, -1)) {
        lua_settop(m_L, m_top0);
        return;
    }
    lua_remove(m_L, -2);  // 去掉 hooks 表，只留函数
    m_valid = true;
}

CLuaHookCall::~CLuaHookCall()
{
    if (m_L) lua_settop(m_L, m_top0);
}

CLuaHookCall& CLuaHookCall::add_object(CMainObject* obj)
{
    if (m_valid) lua_bind_push_object(m_L, obj);
    return *this;
}

CLuaHookCall& CLuaHookCall::add_int(int64_t value)
{
    if (m_valid) lua_pushinteger(m_L, (lua_Integer)value);
    return *this;
}

CLuaHookCall& CLuaHookCall::add_bool(bool value)
{
    if (m_valid) lua_pushboolean(m_L, value ? 1 : 0);
    return *this;
}

CLuaHookCall& CLuaHookCall::add_str(const char* value)
{
    if (m_valid) {
        if (value)
            lua_pushstring(m_L, value);
        else
            lua_pushnil(m_L);
    }
    return *this;
}

CLuaHookCall& CLuaHookCall::begin_table()
{
    if (m_valid) lua_newtable(m_L);
    return *this;
}

CLuaHookCall& CLuaHookCall::table_set_str(const char* key, const char* value)
{
    if (m_valid) {
        lua_pushstring(m_L, value ? value : "");
        lua_setfield(m_L, -2, key);
    }
    return *this;
}

CLuaHookCall& CLuaHookCall::table_set_int(const char* key, int64_t value)
{
    if (m_valid) {
        lua_pushinteger(m_L, (lua_Integer)value);
        lua_setfield(m_L, -2, key);
    }
    return *this;
}

CLuaHookCall& CLuaHookCall::end_table()
{
    // table 已在栈顶，作为实参保留；仅用于调用点可读性。
    return *this;
}

bool CLuaHookCall::call()
{
    if (!m_valid || m_called) return false;

    int nargs = lua_gettop(m_L) - m_top0 - 1;
    if (nargs < 0) nargs = 0;

    if (lua_pcall(m_L, nargs, 1, 0) != LUA_OK) {
        LOG_ERROR("[LuaHook] call failed: %s", lua_tostring(m_L, -1));
        lua_settop(m_L, m_top0);
        m_valid = false;
    }
    m_called = true;
    return m_valid;
}

bool CLuaHookCall::result_is_nil() const
{
    if (!m_valid || !m_called) return true;
    return lua_isnil(m_L, m_top0 + 1) != 0;
}

bool CLuaHookCall::result_bool() const
{
    if (!m_valid || !m_called) return false;
    return lua_toboolean(m_L, m_top0 + 1) != 0;
}

bool CLuaHookCall::result_is_number() const
{
    if (!m_valid || !m_called) return false;
    return lua_isnumber(m_L, m_top0 + 1) != 0;
}

int64_t CLuaHookCall::result_int() const
{
    if (!m_valid || !m_called) return 0;
    if (!lua_isnumber(m_L, m_top0 + 1)) return 0;
    return (int64_t)lua_tointeger(m_L, m_top0 + 1);
}

const char* CLuaHookCall::result_str() const
{
    if (!m_valid || !m_called) return nullptr;
    if (!lua_isstring(m_L, m_top0 + 1)) return nullptr;
    return lua_tostring(m_L, m_top0 + 1);
}

bool CLuaHookCall::table_field_bool(const char* key, bool& out) const
{
    if (!m_valid || !m_called || !key) return false;
    if (!lua_istable(m_L, m_top0 + 1)) return false;
    lua_getfield(m_L, m_top0 + 1, key);
    if (lua_isnil(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    out = lua_toboolean(m_L, -1) != 0;
    lua_pop(m_L, 1);
    return true;
}

bool CLuaHookCall::table_field_int(const char* key, int64_t& out) const
{
    if (!m_valid || !m_called || !key) return false;
    if (!lua_istable(m_L, m_top0 + 1)) return false;
    lua_getfield(m_L, m_top0 + 1, key);
    if (!lua_isnumber(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    out = (int64_t)lua_tointeger(m_L, -1);
    lua_pop(m_L, 1);
    return true;
}

bool CLuaHookCall::table_field_str(const char* key, std::string& out) const
{
    if (!m_valid || !m_called || !key) return false;
    if (!lua_istable(m_L, m_top0 + 1)) return false;
    lua_getfield(m_L, m_top0 + 1, key);
    if (!lua_isstring(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    const char* s = lua_tostring(m_L, -1);
    if (s) out = s;
    lua_pop(m_L, 1);
    return true;
}

int CLuaHookCall::table_str_count() const
{
    if (!m_valid || !m_called) return 0;
    if (!lua_istable(m_L, m_top0 + 1)) return 0;
    return (int)lua_rawlen(m_L, m_top0 + 1);
}

bool CLuaHookCall::table_str_at(int index, std::string& out) const
{
    if (!m_valid || !m_called || index < 0) return false;
    if (!lua_istable(m_L, m_top0 + 1)) return false;
    lua_rawgeti(m_L, m_top0 + 1, (lua_Integer)index + 1);
    if (!lua_isstring(m_L, -1)) {
        lua_pop(m_L, 1);
        return false;
    }
    const char* s = lua_tostring(m_L, -1);
    if (s) out = s;
    lua_pop(m_L, 1);
    return true;
}

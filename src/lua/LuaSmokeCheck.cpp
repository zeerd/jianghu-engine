/** @file LuaSmokeCheck.cpp @brief 钩子冒烟实现（契约见 LuaSmokeCheck.h） */

#include <windows.h>

#include "LuaSmokeCheck.h"

#include <stdio.h>

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <lua.hpp>

#include "Char.h"
#include "Container.h"
#include "HeartBeat.h"  // g_destruct_list：脚本 destruct 后的回收口径
#include "Item.h"
#include "ItemMgr.h"    // create_item_mgr：无头进程的物品注册表
#include "LuaEngine.h"  // g_LuaEngine
#include "LuaItem.h"    // CLuaItem
#include "Room.h"

namespace {

using Clock = std::chrono::steady_clock;

const char* kSmokeLog
    = "__smoke_log";  // registry：日志表，元素 {hook, message}
const char* kSmokeHook = "__smoke_hook";  // registry：当前驱动阶段（出错归谁）

long long ms_since(Clock::time_point since)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()
                                                                 - since)
        .count();
}

// 违规输出必须单行：错误信息里的换行压成空格
std::string one_line(const char* msg)
{
    std::string s = (msg && *msg) ? msg : "unknown error";
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == '\n' || s[i] == '\r') s[i] = ' ';
    return s;
}

void set_phase(lua_State* L, const char* hook)
{
    lua_pushstring(L, hook);
    lua_setfield(L, LUA_REGISTRYINDEX, kSmokeHook);
}

// 记一条错误：{当前阶段, message} 追加进日志表。
// 日志表与阶段名由冒烟主流程在每次驱动前建好——包装层只管追加。
void record_error(lua_State* L, const char* msg)
{
    lua_getfield(L, LUA_REGISTRYINDEX, kSmokeLog);
    lua_createtable(L, 0, 2);  // [log][entry]
    lua_getfield(L, LUA_REGISTRYINDEX, kSmokeHook);
    lua_rawseti(L, -2, 1);  // entry[1] = 阶段名
    lua_pushstring(L, one_line(msg).c_str());
    lua_rawseti(L, -2, 2);  // entry[2] = 错误信息
    lua_rawseti(L, -2, (int)lua_rawlen(L, -2) + 1);
    lua_pop(L, 1);
}

// 钩子包装：原函数出错时先记现场再原样上抛——引擎侧的 lua_pcall 照常
// 吞掉它，冒烟却已拿到证据。upvalue1 = 原函数。
int smoke_hook_wrapper(lua_State* L)
{
    const int nargs = lua_gettop(L);
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_insert(L, 1);
    if (lua_pcall(L, nargs, LUA_MULTRET, 0) != LUA_OK) {
        record_error(L, lua_tostring(L, -1));  // 栈顶仍是错误信息
        return lua_error(L);
    }
    return lua_gettop(L);
}

// pcall / xpcall 包装：返回值语义不变（false, err 照旧），但把被吞掉的
// 错误一并记下——这正是静态防线看不见的那一层。阶段名临时切成 pcall。
int smoke_pcall_wrapper(lua_State* L)
{
    const int nargs = lua_gettop(L);
    // 旧阶段挪到栈底：pcall 期间不能夹在"函数与实参"中间，否则 lua_pcall
    // 会拿错函数（它只认 top-narg 处的函数）。
    lua_getfield(L, LUA_REGISTRYINDEX, kSmokeHook);
    lua_insert(L, 1);
    lua_pushstring(L, "pcall");
    lua_setfield(L, LUA_REGISTRYINDEX, kSmokeHook);

    lua_pushvalue(L, lua_upvalueindex(1));  // 原 pcall
    lua_insert(L, 2);                       // [旧][原][实参...]
    const bool ok = (lua_pcall(L, nargs, LUA_MULTRET, 0) == LUA_OK);

    // 栈：[旧][返回值...]（pcall 只吃掉函数与实参）。先记录——此时阶段名
    // 还是 pcall，正是"被脚本自己吞掉"这一类的标记；随后才还原阶段。
    if (!ok) {
        record_error(L, lua_tostring(L, -1));
    }
    else if (lua_gettop(L) >= 3 && !lua_toboolean(L, 2) && lua_isstring(L, 3)) {
        record_error(L, lua_tostring(L, 3));
    }

    lua_pushvalue(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, kSmokeHook);

    lua_remove(L, 1);              // 弹旧阶段
    if (!ok) return lua_error(L);  // 栈顶是错误信息
    return lua_gettop(L);
}

// 用记录版替换全局 pcall / xpcall；restore 幂等，显式调用与析构双保险
class PcallGuard {
public:
    explicit PcallGuard(lua_State* L) : m_L(L)
    {
        for (int i = 0; i < 2; ++i) {
            lua_getglobal(m_L, kNames[i]);
            if (!lua_isfunction(m_L, -1)) {
                lua_pop(m_L, 1);
                continue;
            }
            lua_pushcclosure(m_L, smoke_pcall_wrapper, 1);
            lua_setglobal(m_L, kNames[i]);
            m_replaced[i] = true;
        }
    }

    ~PcallGuard() { restore(); }

    void restore()
    {
        for (int i = 0; i < 2; ++i) {
            if (!m_replaced[i]) continue;
            lua_getglobal(m_L, kNames[i]);
            if (!lua_isfunction(m_L, -1)) {
                lua_pop(m_L, 1);
                m_replaced[i] = false;
                continue;
            }
            if (lua_getupvalue(m_L, -1, 1) == NULL) {
                lua_pop(m_L, 1);
                m_replaced[i] = false;
                continue;
            }
            lua_insert(m_L, -2);  // [原][包装]
            lua_pop(m_L, 1);      // [原]
            lua_setglobal(m_L, kNames[i]);
            m_replaced[i] = false;  // 再调一次是 no-op
        }
    }

    PcallGuard(const PcallGuard&)            = delete;
    PcallGuard& operator=(const PcallGuard&) = delete;

private:
    static const char* kNames[2];
    lua_State* m_L;
    bool m_replaced[2] = {false, false};
};

const char* PcallGuard::kNames[2] = {"pcall", "xpcall"};

// 把脚本表字段换成"记录后原样上抛"的包装；原函数留在栈顶供还原。
// 字段不是函数则不包装（返回 false，栈不变）。
bool wrap_field(lua_State* L, int table_idx, const char* field)
{
    lua_getfield(L, table_idx, field);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    lua_pushvalue(L, -1);  // 留作还原的那份
    lua_pushcclosure(L, smoke_hook_wrapper, 1);
    lua_setfield(L, table_idx, field);
    return true;
}

// 逆序还原；只在当初包装成功时有效（此时栈顶就是那份原函数）
void unwrap_field(lua_State* L, int table_idx, const char* field, bool wrapped)
{
    if (!wrapped) return;
    lua_pushvalue(L, -1);
    lua_setfield(L, table_idx, field);
    lua_pop(L, 1);
}

// 脚本名 = 文件名去掉 .lua：引擎 scan_dir 按平铺名注册 item 脚本，
// 子目录不进 key（item/food/backleg.lua → backleg）。
std::string bare_name(const std::string& path)
{
    const size_t slash = path.find_last_of("/\\");
    std::string base
        = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (base.size() > 4 && base.compare(base.size() - 4, 4, ".lua") == 0)
        base.resize(base.size() - 4);
    return base;
}

// 点名 = 从 item 目录起的相对路径（item.food.backleg），与 require 目标对齐；
// 不在 item 目录下返回空串，同时充当"是不是数据键"的过滤器。
std::string dotted_name(const std::string& path)
{
    size_t pos = path.find("/item/");
    if (pos == std::string::npos) pos = path.find("\\item\\");
    if (pos == std::string::npos) return std::string();
    std::string rest = path.substr(pos + 6);
    for (size_t i = 0; i < rest.size(); ++i)
        if (rest[i] == '/' || rest[i] == '\\') rest[i] = '.';
    if (rest.size() > 4 && rest.compare(rest.size() - 4, 4, ".lua") == 0)
        rest.resize(rest.size() - 4);
    return "item." + rest;
}

// 本批文件里的字面量 require 目标：被 require 的是模块（库），不是数据键，
// 冒烟它们等于把"库没有 create"当正常情况跑一遍。
void collect_required(const std::vector<std::string>& files,
                      std::set<std::string>& out)
{
    for (const std::string& f : files) {
        FILE* fp = fopen(f.c_str(), "rb");
        if (!fp) continue;
        std::string text;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) text.append(buf, n);
        fclose(fp);

        size_t pos = 0;
        while ((pos = text.find("require", pos)) != std::string::npos) {
            size_t i = pos + 7;
            while (i < text.size()
                   && (text[i] == ' ' || text[i] == '\t' || text[i] == '\n'
                       || text[i] == '\r' || text[i] == '('))
                ++i;
            if (i < text.size() && (text[i] == '"' || text[i] == '\'')) {
                const size_t end = text.find(text[i], i + 1);
                if (end != std::string::npos)
                    out.insert(text.substr(i + 1, end - i - 1));
            }
            pos += 7;
        }
    }
}

// 收割日志（读完即清空，供下个脚本），逐条并进报告
void take_errors(lua_State* L, const std::string& file, SmokeReport& rep)
{
    lua_getfield(L, LUA_REGISTRYINDEX, kSmokeLog);
    if (lua_istable(L, -1)) {
        const int n = (int)lua_rawlen(L, -1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, -1, i);
            SmokeViolation v;
            v.file = file;
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1);
                v.hook = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 2);
                v.message = lua_tostring(L, -1) ? lua_tostring(L, -1) : "";
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            rep.violations.push_back(v);
        }
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kSmokeLog);
}

// 物品注册表守卫：destruct(self) 要按 id 走 g_item_mgr->remove_item，而校验
// 这类无头进程默认没有管理器。与服务器同源地装一个，冒烟才跑在和生产一致的
// 对象生命周期里；调用方已注入（测试的 Mock）时沿用，不碰别人的注入。
struct ItemMgrGuard {
    std::unique_ptr<IItemMgr> owned;

    ItemMgrGuard()
    {
        if (!g_item_mgr) {
            owned      = create_item_mgr();
            g_item_mgr = owned.get();
        }
    }

    ~ItemMgrGuard()
    {
        if (!owned) return;
        owned->item_release();  // 收掉冒烟期间 load_item 注册出来的物件
        g_item_mgr = nullptr;
    }

    ItemMgrGuard(const ItemMgrGuard&)            = delete;
    ItemMgrGuard& operator=(const ItemMgrGuard&) = delete;
};

// 冒烟现场：房间 + 使用者 + 目标。出作用域即解除挂载并把对象从延迟
// 销毁队列摘掉——校验进程没有主循环冲刷，留着会 double-free。
struct SmokeEnv {
    CLuaItem item;
    CRoom room;
    CChar me;
    CContainer target;

    explicit SmokeEnv(const char* script) : item(script)
    {
        item.set_name("smoke_item", "smoke_item");
        room.set("name", "smoke_room");
        room.set("infinity", 1);
        me.set_name("smoke_user", "smoke_user");
        me.set("hp", (long)100);
        me.move(&room);
        target.set_name("smoke_target", "smoke_target");
    }

    ~SmokeEnv() { shutdown(); }

    // 幂等：显式调用一次收尾，析构再调是 no-op
    void shutdown()
    {
        if (!m_live) return;
        m_live = false;
        target.move(nullptr);
        item.move(nullptr);
        me.move(nullptr);
        release(&item);
        release(&me);
        release(&target);
        release(&room);
    }

    static void release(CContainer* ob)
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        g_destruct_list.remove(ob);
    }

private:
    bool m_live = true;
};

// 驱动单个脚本：加载 → 包装 create/do_use → 真调用 → 收割 → 还原
void smoke_one(lua_State* L, SmokeReport& rep, const std::string& file,
               const std::string& name)
{
    const Clock::time_point t0 = Clock::now();

    set_phase(L, "load");
    const int ref = g_LuaEngine->load_script(name.c_str(), LUA_SCRIPT_ITEM);
    if (ref == LUA_NOREF) {  // 模块体执行期出错（语法错已在防线1报过）
        SmokeViolation v;
        v.file    = file;
        v.hook    = "load";
        v.message = one_line(
            g_LuaEngine->get_script_error(name.c_str(), LUA_SCRIPT_ITEM));
        rep.violations.push_back(v);
        return;
    }

    SmokeEnv env(name.c_str());

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);  // 脚本表
    const int tbl             = lua_gettop(L);
    const bool wrapped_create = wrap_field(L, tbl, "create");
    const bool wrapped_douse  = wrap_field(L, tbl, "do_use");
    rep.hooks += (wrapped_create ? 1 : 0) + (wrapped_douse ? 1 : 0);

    set_phase(L, "create");
    env.item.create(0);
    set_phase(L, "do_use");
    env.item.do_use(&env.me, &env.target);

    unwrap_field(L, tbl, "do_use", wrapped_douse);  // 与包装逆序
    unwrap_field(L, tbl, "create", wrapped_create);
    lua_settop(L, tbl - 1);
    set_phase(L, "-");

    env.shutdown();  // 先收现场（析构再调是 no-op），再收割日志
    take_errors(L, file, rep);

    const long long ms = ms_since(t0);
    if (ms > rep.slowest_ms) {
        rep.slowest_ms     = ms;
        rep.slowest_script = file;
    }
}

}  // namespace

SmokeReport smoke_check_items(lua_State* L,
                              const std::vector<std::string>& files)
{
    SmokeReport rep;
    const Clock::time_point t_begin = Clock::now();

    // 冒烟要拿真实对象驱动钩子，只能借引擎自己的状态（registry 里的脚本引用
    // 也挂在它上面）。传进来的不是引擎状态（如单测的裸自检调用）就跳过，
    // 宁可不查也不产假违规。
    if (!g_LuaEngine || g_LuaEngine->get_state() != L) return rep;

    ItemMgrGuard item_mgr_guard;  // 注册表先就位：destruct 要按 id 注销
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kSmokeLog);
    set_phase(L, "-");
    PcallGuard pcall_guard(L);

    std::set<std::string> required;
    collect_required(files, required);

    for (const std::string& file : files) {
        const std::string dotted = dotted_name(file);
        if (dotted.empty()) continue;  // 不在 item 目录 → 不是数据键
        if (required.count(dotted)) {
            ++rep.skipped;
            continue;
        }
        ++rep.scripts;
        smoke_one(L, rep, file, bare_name(file));
    }

    pcall_guard.restore();  // 显式还原（析构再调是 no-op）
    rep.elapsed_ms = ms_since(t_begin);
    return rep;
}

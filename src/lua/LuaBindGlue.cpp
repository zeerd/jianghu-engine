/** @file LuaBindGlue.cpp @brief cross-class Lua glue (call_out, base_*,
 * item-list iterator). These bind functions dispatch over many Lua template
 * classes, so they are not tied to one game module cpp. */

#include "LuaBindIncludes.h"
#include "IniParser.h"
#include "ServerConfig.h"

// 迭代器内部状态结构

typedef DTItemList::iterator ListIterType;

struct ListIterState {
    DTItemList* list = nullptr;

    ListIterType pos;
};

// 迭代器闭包：每次 for-in 循环调用一次

// 返回下一个对象，遍历完毕返回 nil

int l_iter_items_next(lua_State* L)

{
    // upvalue index 1 = ListIterState*

    ListIterState* st = (ListIterState*)lua_touserdata(L, lua_upvalueindex(1));

    if (!st || !st->list || st->pos == st->list->end()) {
        lua_pushnil(L);

        return 1;
    }

    CContainer* item = *st->pos;

    ++st->pos;

    lua_bind_push_object(L, item);

    return 1;
}

// iter_items(list) -> next_func

// 接受 jianghu_item_list userdata，返回一个闭包作为 for-in 迭代器

int l_iter_items(lua_State* L)

{
    DTItemList** ud = (DTItemList**)luaL_checkudata(L, 1, "jianghu_item_list");

    if (!ud || !*ud) {
        // 列表为空，返回 nil（for-in 不执行循环体）

        return 0;
    }

    // 创建迭代器状态作为闭包的 upvalue

    ListIterState* st

        = (ListIterState*)lua_newuserdata(L, sizeof(ListIterState));

    st->list = *ud;

    new (&st->pos) ListIterType((*ud)->begin());

    // 返回闭包，捕获 st 作为唯一 upvalue

    lua_pushcclosure(L, l_iter_items_next, 1);

    return 1;
}

static void lua_callout_bridge(CContainer* ob, int32_t param1, int32_t param2)

{
    if (!ob) {
        LOG_WARN("[Callout] Bridge: ob is NULL, id=%ld, skip", param1);

        return;
    }

    // param1 是回调 ID，param2 不使用（用户参数已存储在注册表中）

    g_LuaEngine->execute_callout(ob, param1);
}

// 从对象反查其 Lua 脚本引用（各类 Lua 对象）

static int resolve_script_ref(CMainObject* obj)

{
    if (!obj) return LUA_NOREF;

    // 技能

    CLuaSkill* skill = dynamic_cast<CLuaSkill*>(obj);

    if (skill) return skill->get_script_ref();

    // NPC（各基类模板实例）

    CLuaNpc* npc = dynamic_cast<CLuaNpc*>(obj);

    if (npc) return npc->get_script_ref();

    CLuaMirrorNpc* mirrornpc = dynamic_cast<CLuaMirrorNpc*>(obj);

    if (mirrornpc) return mirrornpc->get_script_ref();

    // 房间

    CLuaRoom* room = dynamic_cast<CLuaRoom*>(obj);

    if (room) return room->get_script_ref();

    CLuaFBRoom* fbrooom = dynamic_cast<CLuaFBRoom*>(obj);

    if (fbrooom) return fbrooom->get_script_ref();

    CLuaFB* fb = dynamic_cast<CLuaFB*>(obj);

    if (fb) return fb->get_script_ref();

    // 物品（各基类模板实例）

    CLuaItem* item = dynamic_cast<CLuaItem*>(obj);

    if (item) return item->get_script_ref();

    CLuaEquip* equip = dynamic_cast<CLuaEquip*>(obj);

    if (equip) return equip->get_script_ref();

    return LUA_NOREF;
}

// self:call_out(func, delay, param1, param2)
// func 可以是函数名（在 owner 自己的脚本表中按名查找，注册期校验存在性），
// 也可以是函数值（模块定义的回调直接传函数，registry 引用由注册表持有）
// 返回回调 ID（成功）或 0（失败）

int l_call_out(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);

    if (!c) return 0;

    bool is_func_value   = lua_isfunction(L, 2);
    const char* funcName = is_func_value ? nullptr : luaL_checkstring(L, 2);

    time_t delay = (time_t)luaL_checkinteger(L, 3);

    int32_t param1 = (int32_t)luaL_optinteger(L, 4, 0);

    int32_t param2 = (int32_t)luaL_optinteger(L, 5, 0);

    // 获取脚本引用（从 self 反查）

    int scriptRef = resolve_script_ref(obj);

    if (scriptRef == LUA_NOREF) {
        // self 不是 Lua 对象（如玩家）：回退到当前正在执行的脚本

        scriptRef = g_LuaEngine->get_active_script_ref();
    }

    if (scriptRef == LUA_NOREF) return 0;

    int calloutId;

    if (is_func_value) {
        // 函数值：复制一份入 registry，所有权移交注册表
        lua_pushvalue(L, 2);
        int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
        calloutId   = g_LuaEngine->register_callout_function(
            c, scriptRef, funcRef, param1, param2);
        if (calloutId < 0) {
            luaL_unref(L, LUA_REGISTRYINDEX, funcRef);
            return 0;
        }
    }
    else {
        // 注册回调（存储用户参数），归属发起 call_out 的对象

        calloutId = g_LuaEngine->register_callout(c, scriptRef, funcName,
                                                  param1, param2);

        if (calloutId < 0) return 0;
    }

    // 调用 call_out，pFunc 是桥接函数，param1 是回调 ID

    c->call_out(lua_callout_bridge, delay, calloutId, 0);

    lua_pushinteger(L, calloutId);

    return 1;
}

// self:remove_call_out(funcName)

// 移除指定函数名的 call_out

int l_remove_call_out(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);

    if (!c) return 0;

    const char* funcName = luaL_checkstring(L, 2);

    // 调用 LuaEngine 的移除方法

    int removed = g_LuaEngine->remove_callout_by_func_name(c, funcName);

    lua_pushinteger(L, removed);

    return 1;
}

// self:call_out_combat(funcName, delay, param1, param2)

// 战斗场景延迟回调（等价 C++ 的 ob->call_out_combat(pFunc, delay, p1, p2)）

// 回调签名：function(self, param1, param2)

int l_call_out_combat(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);

    if (!c) return 0;

    const char* funcName = luaL_checkstring(L, 2);

    time_t delay = (time_t)luaL_checkinteger(L, 3);

    int32_t param1 = (int32_t)luaL_optinteger(L, 4, 0);

    int32_t param2 = (int32_t)luaL_optinteger(L, 5, 0);

    int scriptRef = resolve_script_ref(obj);

    if (scriptRef == LUA_NOREF) {
        scriptRef = g_LuaEngine->get_active_script_ref();
    }

    if (scriptRef == LUA_NOREF) return 0;

    int calloutId

        = g_LuaEngine->register_callout(c, scriptRef, funcName, param1, param2);

    if (calloutId < 0) return 0;

    c->call_out_combat(lua_callout_bridge, delay, calloutId, 0);

    lua_pushinteger(L, calloutId);

    return 1;
}

// self:base_handle_action(comm, me, arg) - 调用基类 3 参 handle_action

// 注意：必须用 dynamic_cast + 非虚成员调用，避免 CContainer* 虚调用

// 回到 CLuaNpcT<T>::handle_action 造成 C↔Lua 无限递归（C stack overflow）

int l_base_handle_action(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    const char* comm = luaL_checkstring(L, 2);

    CMainObject* meObj = get_object(L, 3);

    CChar* me = meObj ? dynamic_cast<CChar*>(meObj) : NULL;

    if (!me) return 0;

    const char* arg = luaL_optstring(L, 4, "");

    if (CLuaNpc* n = dynamic_cast<CLuaNpc*>(obj)) {
        lua_pushinteger(L, n->base_handle_action((char*)comm, me, (char*)arg));

        return 1;
    }

    lua_pushinteger(L, 0);

    return 1;
}

// self:base_do_talk(me, ask) - 调用基类 do_talk

// 与 l_base_handle_action 同理：dynamic_cast 到每个 CLuaNpcT 具体类型后

// 非虚调用 base_do_talk，避免虚调用回到 CLuaNpcT<T>::do_talk 造成 C↔Lua 递归。

int l_base_do_talk(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    CChar* me = dynamic_cast<CChar*>(get_object(L, 2));

    const char* ask = luaL_optstring(L, 3, "");

    if (CLuaVendor* n = dynamic_cast<CLuaVendor*>(obj)) {
        lua_pushinteger(L, n->base_do_talk(me, (char*)ask));

        return 1;
    }

    if (CLuaBankOwner* n = dynamic_cast<CLuaBankOwner*>(obj)) {
        lua_pushinteger(L, n->base_do_talk(me, (char*)ask));

        return 1;
    }

    if (CLuaMirrorNpc* n = dynamic_cast<CLuaMirrorNpc*>(obj)) {
        lua_pushinteger(L, n->base_do_talk(me, (char*)ask));

        return 1;
    }

    if (CLuaNpc* n = dynamic_cast<CLuaNpc*>(obj)) {
        lua_pushinteger(L, n->base_do_talk(me, (char*)ask));

        return 1;
    }

    lua_pushinteger(L, 0);

    return 1;
}

// handle_action(obj, comm, me, arg) - 全局函数：虚调用对象的 3 参 handle_action

// 供物品/其他脚本把命令转发给 NPC（如战报把 lookup/opench 转给登记员），

// Lua 定义的 NPC 会正常进入其脚本 handle_action 钩子。

// 与 base_handle_action 的区别：后者是非虚调用 C++ 基类实现。

int l_handle_action(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    const char* comm = luaL_checkstring(L, 2);

    CChar* me = obj ? dynamic_cast<CChar*>(get_object(L, 3)) : NULL;

    const char* arg = luaL_optstring(L, 4, "");

    CContainer* con = obj ? dynamic_cast<CContainer*>(obj) : NULL;

    if (!con || !me) {
        lua_pushinteger(L, 0);

        return 1;
    }

    IActionHandler* handler = dynamic_cast<IActionHandler*>(con);
    lua_pushinteger(
        L, handler ? handler->handle_action((char*)comm, me, (char*)arg) : 0);

    return 1;
}

// self:base_create() - 调用基类 create()

// NPC 脚本（CLuaMirrorNpc）在 create 中复用 CNpc::create 的默认实现。

int l_base_create(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    if (CLuaMirrorNpc* mi = dynamic_cast<CLuaMirrorNpc*>(obj)) {
        mi->base_create();

        return 0;
    }

    return 0;
}

// self:base_handle_action4(comm, me, arg, tar) - 调用基类 4 参 handle_action

// NPC（CLuaNpcT）与房间（CLuaRoomT）脚本接管 handle_action 后均可调用

int l_base_handle_action4(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    const char* comm = luaL_checkstring(L, 2);

    CMainObject* meObj = get_object(L, 3);

    CChar* me = meObj ? dynamic_cast<CChar*>(meObj) : NULL;

    if (!me) return 0;

    const char* arg = luaL_optstring(L, 4, "");

    const char* tar = luaL_optstring(L, 5, "");

    // 房间系（CRoom/CFBRoom 均有 4 参 handle_action）

    if (CLuaRoom* r = dynamic_cast<CLuaRoom*>(obj)) {
        lua_pushinteger(

            L, r->base_handle_action4((char*)comm, me, (char*)arg, (char*)tar));

        return 1;
    }

    if (CLuaFBRoom* fr = dynamic_cast<CLuaFBRoom*>(obj)) {
        lua_pushinteger(L, fr->base_handle_action4((char*)comm, me, (char*)arg,

                                                   (char*)tar));

        return 1;
    }

    if (CLuaNpc* n = dynamic_cast<CLuaNpc*>(obj)) {
        lua_pushinteger(

            L, n->base_handle_action4((char*)comm, me, (char*)arg, (char*)tar));

        return 1;
    }

    lua_pushinteger(L, 0);

    return 1;
}

// self:base_init(me) - 调用基类 init()（自动战斗/仇恨/挑衅等逻辑）

// CLuaNpcT<T>::init 在有 Lua 脚本时不调 T::init(me)，需手动调用。

// 物品脚本（CLuaItemT）接管 init 后调用 self:base_init() 复用

// CItem::init（反复制检查/重量同步），物品 init 无 me 参数。

// 房间脚本（CLuaRoomT）接管 init 后同样调用 self:base_init(me) 复用基类 init。

int l_base_init(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    if (CLuaItem* it = dynamic_cast<CLuaItem*>(obj)) {
        it->base_init();

        return 0;
    }

    if (CLuaEquip* eq = dynamic_cast<CLuaEquip*>(obj)) {
        eq->base_init();

        return 0;
    }

    CMainObject* meObj = get_object(L, 2);

    CChar* me = meObj ? dynamic_cast<CChar*>(meObj) : NULL;

    if (CLuaNpc* n = dynamic_cast<CLuaNpc*>(obj)) {
        n->base_init(me);

        return 0;
    }

    // 房间系（CRoom/CFBRoom/CFB 均有 init(CChar*)）
    if (CLuaRoom* r = dynamic_cast<CLuaRoom*>(obj)) {
        r->base_init(me);

        return 0;
    }

    if (CLuaFBRoom* fr = dynamic_cast<CLuaFBRoom*>(obj)) {
        fr->base_init(me);

        return 0;
    }

    if (CLuaFB* fb = dynamic_cast<CLuaFB*>(obj)) {
        fb->base_init(me);

        return 0;
    }

    return 0;
}

// self:base_valid_leave(me, dir) - 调用基类 valid_leave（非虚限定调用防递归）

// 房间脚本（CLuaRoomT）接管 valid_leave 后需要基类放行/拦截判定时使用；

// 非房间对象无基类可调，返回 0。

int l_base_valid_leave(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) {
        lua_pushinteger(L, 0);

        return 1;
    }

    CMainObject* meObj = get_object(L, 2);

    CChar* me = meObj ? dynamic_cast<CChar*>(meObj) : NULL;

    const char* dir = luaL_optstring(L, 3, "");

    if (CLuaRoom* r = dynamic_cast<CLuaRoom*>(obj)) {
        lua_pushinteger(L, r->base_valid_leave(me, dir));

        return 1;
    }

    if (CLuaFBRoom* fr = dynamic_cast<CLuaFBRoom*>(obj)) {
        lua_pushinteger(L, fr->base_valid_leave(me, dir));

        return 1;
    }

    if (CLuaFB* fb = dynamic_cast<CLuaFB*>(obj)) {
        lua_pushinteger(L, fb->base_valid_leave(me, dir));

        return 1;
    }

    lua_pushinteger(L, 0);

    return 1;
}

// self:base_setup() - 调用基类 setup()（注册询问等逻辑）

// CLuaNpcT<T>::setup 在有 Lua 脚本时不调 T::setup()，需手动调用。

// 房间脚本（CLuaRoomT）接管 setup 后调用 self:base_setup() 复用

// C++ 基类 setup（map/area/city 等关键属性）。

int l_base_setup(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    if (CLuaRoom* r = dynamic_cast<CLuaRoom*>(obj)) {
        r->base_setup();

        return 0;
    }

    if (CLuaFBRoom* fr = dynamic_cast<CLuaFBRoom*>(obj)) {
        fr->base_setup();

        return 0;
    }

    if (CLuaFB* fb = dynamic_cast<CLuaFB*>(obj)) {
        fb->base_setup();

        return 0;
    }

    if (CLuaNpc* n = dynamic_cast<CLuaNpc*>(obj)) {
        n->base_setup();

        return 0;
    }

    if (CLuaVendor* ve = dynamic_cast<CLuaVendor*>(obj)) {
        ve->base_setup();

        return 0;
    }

    if (CLuaBankOwner* bk = dynamic_cast<CLuaBankOwner*>(obj)) {
        bk->base_setup();

        return 0;
    }

    return 0;
}

// self:base_reset() - 调用 C++ 基类 reset()（NPC 复位/物品清理/房产税金）

// CLuaRoomT<T>::reset 在脚本定义 reset 时不调 T::reset()，脚本需手动调用

int l_base_reset(lua_State* L)

{
    CMainObject* obj = get_object(L, 1);

    if (!obj) return 0;

    if (CLuaRoom* r = dynamic_cast<CLuaRoom*>(obj)) {
        r->base_reset();

        return 0;
    }

    if (CLuaFBRoom* fr = dynamic_cast<CLuaFBRoom*>(obj)) {
        fr->base_reset();

        return 0;
    }

    if (CLuaFB* fb = dynamic_cast<CLuaFB*>(obj)) {
        fb->base_reset();

        return 0;
    }

    return 0;
}

// ini_get(section, key, default) -> string - 读取 config.ini 配置值，
// 节与键由脚本自行选择
int l_ini_get(lua_State* L)
{
    const char* section = luaL_checkstring(L, 1);
    const char* key     = luaL_checkstring(L, 2);
    const char* def     = luaL_optstring(L, 3, "");
    lua_pushstring(L, ini_get(section, key, def));
    return 1;
}

// utf8_to_gbk(s) -> string - 脚本侧 UTF-8 文本转成服务器内部 GBK
int l_utf8_to_gbk(lua_State* L)
{
    const char* s = luaL_checkstring(L, 1);
    lua_pushstring(L, utf8_to_gbk(s).c_str());
    return 1;
}

// get_no_force_conflict() -> number - config.ini [server] no_force_conflict
// 开关（开启返回 1，否则返回 0）
int l_get_no_force_conflict(lua_State* L)
{
    lua_pushinteger(L, CServerConfig::no_force_conflict() ? 1 : 0);
    return 1;
}

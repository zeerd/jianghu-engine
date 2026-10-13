/**
 * @file ContainerMisc.cpp
 * @brief CContainer: call_out, menu, save/load/lock,
 * wield/wear.  */

#include <windows.h>
#include <algorithm>
#include "Container.h"
#include "Room.h"
#include "User.h"
#include "Item.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "FightRoom.h"
#include "Channel.h"
#include "Sql.h"

//==== methods ====

// 增加延迟函式
void CContainer::call_out(CALLOUTFUNC func, time_t delay, int32_t param1,
                          int32_t param2)
{
    if (!this) return;

    // 放入延迟函式链表
    if (delay <= 0) return;

    auto p    = std::make_unique<CALLOUTNODE>();
    p->func   = func;
    p->type   = 0;
    p->delay  = delay;
    p->param1 = param1;
    p->param2 = param2;

    m_callout_list.push_front(std::move(p));

    set_heart_beat(1);
}

// 战斗结束时强制执行的延迟函式
void CContainer::call_out_combat(CALLOUTFUNC func, time_t delay, int32_t param1,
                                 int32_t param2)
{
    if (!this) return;

    auto p    = std::make_unique<CALLOUTNODE>();
    p->func   = func;
    p->type   = 1;
    p->delay  = delay;
    p->param1 = param1;
    p->param2 = param2;

    m_callout_list.push_front(std::move(p));

    set_heart_beat(1);
}

// 移除 evalute = 1，执行
void CContainer::remove_call_out(CALLOUTFUNC func, int evalute)
{
    auto it = m_callout_list.begin();
    while (it != m_callout_list.end()) {
        if ((*it)->executing) {  // 执行中的节点由所属心跳在回调后回收
            ++it;
            continue;
        }
        if ((*it)->func == func) {
            // erase 会销毁节点，先拷贝需用的字段再执行回调
            CALLOUTFUNC func = (*it)->func;
            int32_t p1       = (*it)->param1;
            int32_t p2       = (*it)->param2;
            it               = m_callout_list.erase(it);

            // 执行
            if (evalute && func) (*func)(this, p1, p2);
            return;
        }
        ++it;
    }
}

// 按回调 ID（param1）精确移除挂起的 call_out（Lua 桥接回调专用）
void CContainer::remove_call_out_by_id(int32_t calloutId)
{
    auto it = m_callout_list.begin();
    while (it != m_callout_list.end()) {
        if ((*it)->executing) {  // 执行中的节点由所属心跳在回调后回收
            ++it;
            continue;
        }
        if ((*it)->param1 == calloutId) {
            m_callout_list.erase(it);
            return;
        }
        ++it;
    }
}

// 执行并结束所有战斗延迟呼叫
void CContainer::evaluate_call_out_combat()
{
    int32_t object_id = m_object_id;

    auto it = m_callout_list.begin();

    while (it != m_callout_list.end()) {
        if ((*it)->type != 1 || (*it)->executing) {
            ++it;
            continue;
        }

        // 回调可能移除链表中的任意挂起节点，也可能销毁本对象：
        // 节点先留在链表并标记 executing，快照字段后执行，回调结束再回收。
        CALLOUTFUNC func = (*it)->func;
        int32_t p1       = (*it)->param1;
        int32_t p2       = (*it)->param2;
        (*it)->executing = true;

        // 执行
        if (func) (*func)(this, p1, p2);
        // 判断本物件是否在函式中销毁
        if (g_last_destruct_object == object_id) return;

        it = m_callout_list.erase(it);
    }
}

int CContainer::save()
{
    if (m_destruct) return 0;

    return save_impl(&_M);
}

int CContainer::load()
{
    CMapping attr;

    if (!load_impl(&attr)) return 0;
    _M.copy_mapping(&attr);
    return 1;
}

// 持久化路由：dbase 的 save_table/save_key（由 Lua 脚本声明、构造期镜像写入）
// 决定存盘表与该表 obj_id 的取值键；未声明则按对象类型默认。
// C++ 不认识任何数据集专用表。
void CContainer::save_route(std::string& out_table, std::string& out_id)
{
    const char* table = querystr("save_table");
    if (table && table[0]) {
        out_table       = table;
        const char* key = querystr("save_key");
        if (key && key[0])
            out_id = querystr(key);
        else
            out_id = querystr("base_name");
        return;
    }

    if (query_attr(IS_ROOM))
        out_table = "t_room";
    else if (query_attr(IS_CHAR))
        out_table = "t_npc";
    else
        out_table = "t_item";
    out_id = querystr("base_name");
}

// 可存盘之物件
int CContainer::save_impl(CMapping* savedata)
{
    try {
        const char* attr;

        if (!query_attr(CAN_SAVE)) return 0;

        attr = savedata->get_buffer();

        std::string table_name, obj_id;
        save_route(table_name, obj_id);

        // 记录已存在则 UPDATE，否则 INSERT（不能因查不到记录而拒绝写入）
        std::string dummy;
        bool is_update
            = db_load_container(table_name.c_str(), obj_id.c_str(), dummy);

        if (!db_save_container(table_name.c_str(), obj_id.c_str(), name(1),
                               attr, is_update)) {
            return 0;
        }

        return 1;
    } catch (...) {
        LOG_ERROR("saveobject: save");
        return 0;
    }
}

int CContainer::load_impl(CMapping* savedata)
{
    try {
        int32_t len;

        if (!query_attr(CAN_SAVE)) return 0;

        std::string table_name, obj_id;
        save_route(table_name, obj_id);

        std::string data;
        if (!db_load_container(table_name.c_str(), obj_id.c_str(), data)) {
            return 0;
        }

        len = savedata->load_buffer(data.c_str());

#ifdef MORE_LOGS
        LOG_DEBUG("CContainer::_Load : %s", table_name.c_str());
#endif

        if (len == -1) {
            set("fail_ob", 2);
            LOG_ERROR("载入物件%s失败!", querystr("base_name"));
            return 0;
        }

        return 1;
    } catch (...) {
        LOG_ERROR("载入物件%s失败!", querystr("base_name"));
    }
    return 0;
}

// 锁定数据
int CContainer::lock_data(int locked)
{
    int ret  = lock_data_impl(locked);
    int wait = 0;
    time_t ct;
    time(&ct);

    while (!ret && !locked) {
        time_t t;
        if (time(&t) != ct) {
            ct = t;
            if (lock_data_impl(locked)) return 1;
            if (wait++ > 3) return 0;
        }
    }

    return 1;
}

int CContainer::lock_data_impl(int locked)
{
    std::string table_name, obj_id;
    save_route(table_name, obj_id);

    int current_lock = 0;
    if (!db_lock_container(table_name.c_str(), obj_id.c_str(), locked,
                           current_lock)) {
        return 0;
    }

    // 检查是否冲突（db_lock_container 内部已检查，但保留原有的消息输出逻辑）
    if ((locked && current_lock) || (!locked && !current_lock)) {
        char msg[255];
        g_channel.do_channel(
            &g_channel, NULL, "sys",
            snprintf(msg, 255, "%s lock = %d 失败！", name(1), locked));
        return 0;
    }

    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:Save() -> number
int l_save(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (c)
        lua_pushinteger(L, (lua_Integer)c->save());
    else
        lua_pushinteger(L, 0);
    return 1;
}

// self:Load() -> number - 从磁盘重新加载数据（与 Save 对应，房间/容器）
int l_load(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (c)
        lua_pushinteger(L, (lua_Integer)c->load());
    else
        lua_pushinteger(L, 0);
    return 1;
}

// self:LockData(locked) -> number - 锁定/解锁容器数据（帮会/房屋持久化用）
int l_lockdata(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    int locked = (int)luaL_optinteger(L, 2, 1);
    lua_pushinteger(L, (lua_Integer)c->lock_data(locked));
    return 1;
}

// self:DownLoad(me)
int l_download(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    IClientObject* c = dynamic_cast<IClientObject*>(obj);
    if (!c) return 0;
    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (me) c->download(me);
    return 0;
}

// self:wield() -> number - 装备（CContainer::wield）
int l_wield(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    IEquippable* c = dynamic_cast<IEquippable*>(obj);
    if (!c) return 0;
    lua_pushinteger(L, c->wield());
    return 1;
}

// self:wear() -> number - 穿上（CContainer::wear）
int l_wear(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    IEquippable* c = dynamic_cast<IEquippable*>(obj);
    if (!c) return 0;
    lua_pushinteger(L, c->wear());
    return 1;
}

// self:SendMenu(me) - 发送房间菜单
int l_send_menu(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    IDialogSource* c = dynamic_cast<IDialogSource*>(obj);
    if (!c) return 0;

    CChar* me = NULL;
    if (lua_isuserdata(L, 2)) me = dynamic_cast<CChar*>(get_object(L, 2));
    if (me) c->send_menu(me);
    return 0;
}

// self:AddMenuItem(show, cmd, me) - 添加一个菜单项（与 CItem::AddMenuItem
// 等价）
int l_add_menu_item(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    IDialogSource* c = dynamic_cast<IDialogSource*>(obj);
    if (!c) return 0;

    const char* showRaw = luaL_optstring(L, 2, "");
    const char* cmd     = luaL_optstring(L, 3, "");
    CChar* me           = NULL;
    if (lua_isuserdata(L, 4)) me = dynamic_cast<CChar*>(get_object(L, 4));
    if (me) c->add_menu_item(showRaw, cmd, me);
    return 0;
}

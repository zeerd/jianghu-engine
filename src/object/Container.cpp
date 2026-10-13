/**
 * @file Container.cpp
 * @brief implementation of the CContainer class. */

#include <windows.h>
#include <algorithm>
#include "Container.h"
#include "Room.h"
#include "User.h"
#include "Item.h"
#include "Npc.h"
#include "CmdHandle.h"
#include "FightRoom.h"
#include "LuaHook.h"
#include "Channel.h"
#include "Sql.h"

void set_heart_beat_obj(CContainer* obj, int flag);

int32_t g_object_id = 1;  // 房间，物品，NPC，玩家等物件的统一编号。

//==== methods ====
CContainer::~CContainer()  // 环境毁灭，物品毁灭，但玩家将落入外部空间
{
    m_callout_list.clear();
}

// 复制一份链表（从 Global.cpp 迁入）
void copy_list(DTItemList* dest, DTItemList* src)
{
    for (auto obj : *src) {
        dest->push_back(obj);
    }
}

CContainer::CContainer()
{
    m_environment = NULL;
    m_object_id   = g_object_id++;
    m_weight = max_encumb = encumb = 0;
    m_update                       = 1;
    m_destruct                     = false;
}

int32_t CContainer::query_attr(int attr)
{
    switch (attr) {
        case IS_CONTAINER:
            return 1;
        case UPDATE:
            return m_update;
    }
    return CMainObject::query_attr(attr);
}

// 物品缺省补全钩子（hooks.object_create）：数据集按身份补 icon 等缺省
void CContainer::create(int flag)
{
    CLuaHookCall hook("object_create");
    hook.add_object(this);
    hook.add_int(flag);
    hook.call();
}

void CContainer::init(CChar* me) {}

void CContainer::destructed() {}

// 清除内部物件。
void CContainer::clean_up()
{
    DTItemList search;
    char msg[80];

    CRoom* room = load_room("room_void");

    copy_list(&search, &m_item_list);

    for (auto obj2 : search) {
        //      obj->move(room);            //物品落入空房间
        if (userp(obj2))
            obj2->move(room);
        else {
            if (!query("saveobjects")) {
                // 野外盒子里的套装要销毁
                if (is_box()) {
                    obj2->purge();
                }
            }
            DESTRUCT_OB(obj2, snprintf(msg, 80, "%s->Clean_Up", name(1)));
        }
    }
}

// 表示被销毁了。
int CContainer::purge()
{
    // ( )主动退出
    // ( )掉线

    // (V)被泰山捡走
    // (V)给朱熹
    // (V)地上等刷新
    // (V)重启时野外箱子里的
    // (V)重启时地上的
    // (V)重启时家里地上的

    std::string table_name, obj_id;
    save_route(table_name, obj_id);

    // 数据集专用表：物件销毁时同步删除其存档行
    if (querystr("save_table")[0]) {
        CContainer* env = environment();
        LOG_DEBUG("CContainer(%s)::purge(%s): %s",
                  ((env == NULL) ? "-" : env->name(1)), name(1),
                  obj_id.c_str());

        if (!db_delete_container(table_name.c_str(), obj_id.c_str())) {
            LOG_ERROR("CContainer::purge:DELETE db fail %s", obj_id.c_str());
            return 0;
        }
    }
    return 1;
}

int32_t CContainer::object_id() { return m_object_id; }

void CContainer::setup() {}

void CContainer::heart_beat()
{
    LOG_TRACE("IN");
    // 执行延迟函式
    int32_t object_id = m_object_id;
    if (!this) return;

    auto it = m_callout_list.begin();
    while (it != m_callout_list.end()) {
        CALLOUTNODE* node = it->get();

        if (node->executing) {
            ++it;  // 嵌套进入时跳过正在执行的节点，避免重复执行
            continue;
        }

        LOG_DEBUG("%s(%d) delay=%d!", name(1), m_object_id, node->delay);
        if (node->type == 1 && !node->delay) {
            ++it;  // delay为0的表示无限长时间，战斗结束时呼叫
            continue;
        }

        if (--node->delay > 0) {
            ++it;
            continue;
        }

        LOG_DEBUG("%s call_out!", name(1));

        // 回调可能移除链表中的任意挂起节点，也可能销毁本对象：
        // 节点先留在链表并标记 executing，快照字段后执行，回调结束再回收，
        // 这样 it 指向的节点不会在回调中被删而悬垂。
        CALLOUTFUNC func = node->func;
        int32_t p1       = node->param1;
        int32_t p2       = node->param2;
        node->executing  = true;
        if (func) (*func)(this, p1, p2);

        // 判断本物件是否在函式中销毁
        if (g_last_destruct_object == object_id) return;

        it = m_callout_list.erase(it);

        LOG_DEBUG(">> call_out");
    }
    LOG_TRACE("OUT");
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:create([flag])  重新初始化对象（武器强度重算等；CContainer::create
// 默认空实现）
int l_create(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* con = dynamic_cast<CContainer*>(obj);
    if (!con) return 0;

    int flag = (int)luaL_optinteger(L, 2, 0);
    con->create(flag);
    return 0;
}

// self:object_id() -> number
int l_object_id(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (c)
        lua_pushinteger(L, c->object_id());
    else
        lua_pushinteger(L, 0);
    return 1;
}

// self:purge() - 清空容器内所有物品
int l_purge(lua_State* L)
{
    CContainer* c = NULL;
    if (lua_isuserdata(L, 1)) c = dynamic_cast<CContainer*>(get_object(L, 1));
    if (c) c->purge();
    return 0;
}

// self:clear() - 清空整个持久 dbase
int l_clear(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    ch->clear();
    return 0;
}

// self:set_heart_beat(flag)  加入/移除心跳链表（护送类NPC用于定时触发）
int l_set_heart_beat(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CContainer* con = dynamic_cast<CContainer*>(obj);
    if (!con) return 0;

    int flag = (int)luaL_optinteger(L, 2, 1);
    con->set_heart_beat(flag);
    return 0;
}

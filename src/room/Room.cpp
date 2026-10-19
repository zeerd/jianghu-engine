/** @file Room.cpp @brief implementation of the CRoom class. */

#include <windows.h>

#include <algorithm>

#include "Global.h"
#include "Room.h"
#include "Npc.h"
#include "User.h"
#include "LuaEngine.h"
#include "IClientObject.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CRoom::CRoom()
{
    set_name("标准房间");
    set("infinity", 1);  // 不受容量限制
    set("max_items", 30);

    m_box_num = 0;
}

CRoom::~CRoom()
{
    // 释放出口、箱子

    remove_all_doors();

    m_object_list.clear();
}

// 把矩形活动区域写到物件的地图坐标键上，客户端地图按这六键摆放物件。
static void set_map_position(CContainer* obj, int32_t bx, int32_t by,
                             int32_t ex, int32_t ey, int dir, int active)
{
    obj->set("map/bx", bx);
    obj->set("map/by", by);
    obj->set("map/ex", ex);
    obj->set("map/ey", ey);
    obj->set("map/dir", dir);
    obj->set("map/active", active);
}

// 清除房间内非角色物件（跨两遍 wait_clean 标记）。
// 先收集待销毁物件再统一销毁，避免遍历 m_item_list 时 DESTRUCT_OB
// 使迭代器失效。 副本房（CFBRoom）与普通房共用此块。
void CRoom::clean_up_items()
{
    DTItemList toDestruct;
    for (auto item2 : m_item_list) {
        if (item2->is_character()) continue;  // 不清人

        // 设置本房间的等待清除标志，换房间不算
        if (EQUALSTR(querystr("base_name"), "room_void")
            || (!item2->query("no_clean_up")
                && EQUALSTR(item2->querystr_temp("wait_clean"),
                            querystr("base_name")))) {
            item2->purge();
            toDestruct.push_back(item2);
        }
        else
            item2->set_temp("wait_clean", querystr("base_name"));
    }
    for (auto item2 : toDestruct) {
        DESTRUCT_OB(item2, "CRoom::reset");
    }
}

// Update时呼叫。
void CRoom::reset()
{
    CNpc* npc;
    CContainer* item;

    if (!query("no_clean_up")) clean_up_items();
    if (query("no_reset_npc")) return;  // 不复位NPC

    // 查找物件列表，将物件复位
    for (auto& obj : m_object_list) {
        if (obj->is_npc) {
            if (obj->interval > 0) {
                obj->interval--;
                continue;
            }
            obj->interval = obj->base_interval;
            npc           = (CNpc*)present(obj->object);
            if (!npc) npc = find_npc(obj->object);
            if (!npc) {
                npc = load_npc(obj->base_name.c_str());
                npc->move(this);
                obj->object = npc->object_id();

                set_map_position(npc, obj->bx, obj->by, obj->ex, obj->ey,
                                 obj->dir, obj->active);
            }
            else {
                if (npc->environment() != this) npc->return_home(this);
            }
        }
        else {
            item = present(obj->object);
            if (!item) {
                item = g_item_mgr->load_item(obj->base_name.c_str());
                item->move(this);
                obj->object = item->object_id();
            }
        }
    }
}

// 增加NPC
CNpc* CRoom::add_npc(const char* base_name, int32_t bx, int32_t by, int32_t ex,
                     int32_t ey, int dir, int active, int ival)
{
    CNpc* obj;
    auto npc = std::make_unique<OBJECT_NODE>();

    obj = load_npc(base_name);
    obj->move(this);
    npc->object        = obj->object_id();
    npc->base_name     = base_name;
    npc->is_npc        = 1;
    npc->bx            = bx;
    npc->by            = by;
    npc->ex            = ex;
    npc->ey            = ey;
    npc->dir           = dir;
    npc->active        = active;
    npc->interval      = ival;
    npc->base_interval = ival;

    m_object_list.push_front(std::move(npc));
    obj->set("start_city", querystr("base_name"));
    set_map_position(obj, bx, by, ex, ey, dir, active);

    return obj;
}

// 房间在此创建
void CRoom::create() {}

// 门工厂：普通房产 CDoor；副本房重写为产 CFBDoor。
CDoor* CRoom::new_door() { return new CDoor(); }

// 增加出口，给房间内物件下载入口信息
CDoor* CRoom::add_door(const char* id, const char* next_room,
                       const char* NextDoor, const char* dir)
{
    CDoor* door = new_door();

    m_door_list.push_front(door);
    door->set_name(id, id);

    door->set("dir", dir);  // 方向
    door->set("next_room", next_room);
    door->set("next_door", NextDoor);
    door->un_locked();

    // 下载出口信息：动态填加时有用。暂时不加此功能
    return door;
}

// 增加箱子
CBox* CRoom::add_box(const char* box_type)
{
    CBox* box;

    m_box_num++;

    box = (CBox*)g_item_mgr->load_item(box_type);
    box->move(this);

    // 如果本房间是可存盘的，箱子里的物件应该可以被保留
    if (query("saveobjects")) {
        char fname[80];
        box->set("db_room_id", snprintf(fname, 80, "%s_%d",
                                        querystr("base_name"), m_box_num));
        box->set("saveobjects", query("saveobjects"));
        box->load_object();
    }

    return box;
}

int32_t CRoom::query_attr(int attr)
{
    switch (attr) {
        case IS_ROOM:
            return 1;
    }

    return CContainer::query_attr(attr);
}

void CRoom::change_room(CContainer* me)
{
    char msg[255];

    CContainer::change_room(me);

    if (userp(me)) {
        // 进入普通场景清除副本死亡不损失标志；副本房（CFBRoom）保留，
        // 故按 IS_FBROOM 分流——CRoom 由此吸收原 CFBRoom::change_room。
        if (!query_attr(IS_FBROOM)) me->delete_temp("fb_nodie");
        // 传送出口信息
        for (auto door2 : m_door_list) {
            snprintf(msg, 255, "&C=%d&R=%d&D=%s&N=%s&A=%s&", ADDDOOR,
                     object_id(), door2->querystr("id"), door2->name(1),
                     door2->querystr("dir"));
            static_cast<CUser*>(me)->send_command(msg, SEND_WAIT_BUFFERED);
        }

        static_cast<CUser*>(me)->send_off();
    }
}

// 通过某号出口  //id也可以是方向
CDoor* CRoom::present_door(const char* id)
{
    for (auto door : m_door_list) {
        if (EQUALSTR(door->querystr("id"), id)
            || EQUALSTR(door->querystr("dir"), id)) {
            return door;
        }
    }

    return 0;
}

// 当角色进入时执行
void CRoom::init(CChar* me) {}

DTEXITLIST* CRoom::query_exits() { return &m_door_list; }

CContainer* CRoom::add_item(const char* item_name)
{
    CContainer* item;
    auto obj = std::make_unique<OBJECT_NODE>();

    item = g_item_mgr->load_item(item_name);
    item->move(this);
    obj->object    = item->object_id();
    obj->base_name = item_name;
    obj->is_npc    = 0;
    m_object_list.push_front(std::move(obj));

    return item;
}

int CRoom::do_look(CChar* me)
{
    if (querystr("long")[0]) {
        char msg[1024];
        snprintf(msg, 1024, "%s", querystr("long"));
        say(msg, me);
    }
    else
        say("这里没有什么特别的。", me);
    return 0;
}

void CRoom::remove_all_doors()
{
    for (auto door : m_door_list) {
        delete door;
    }
    m_door_list.clear();
}

void CRoom::remove_doors(const char* arg)
{
    auto it = m_door_list.begin();
    while (it != m_door_list.end()) {
        CDoor* door = *it;
        if (EQUALSTR(door->querystr("next_room"), arg)) {
            it = m_door_list.erase(it);
            delete door;
            break;
        }
        ++it;
    }
}

void CRoom::setup()
{
    if (_M.undefinedp("map")) _M.set("map", _M.querystr("base_name"));

    if (_M.undefinedp("area")) {
        char area_name[5];
        strncpy(area_name, _M.querystr("base_name"), 4)[4] = 0;
        set("area", area_name);  // 区域名
    }

    // 设置城市标记
    setup_city(this);
}

int CRoom::valid_leave(CChar* me, const char* dir)
{
    if (query("no_leave")) return 0;  // 副本锁定：禁止离开
    return 1;
}

void CRoom::setup_city(CRoom* city)
{
    // 从 Lua 数据脚本获取城市列表（封装在 LuaEngine 内部，不直接操作 Lua 栈）
    std::vector<std::string> cityList;
    if (g_LuaEngine && g_LuaEngine->get_city_list(cityList)) {
        CMapping room_decode;
        for (const auto& cityName : cityList) {
            room_decode.set(cityName.c_str(), 1);
        }
        city->set("city", room_decode.query(city->querystr("area")));
    }
}

void CRoom::remove_all_box()
{
    DTItemList list;
    copy_list(&list, &m_item_list);

    for (auto obj : list) {
        if (!obj->query_attr(IS_BOX)) continue;

        ((CBox*)obj)->remove_objects();
        DESTRUCT_OB(obj, "CRoom::remove_all_box");
    }

    m_box_num = 0;
}

// Do_Action, handle_action(3参), SendMenu, AddMenuItem, say,
// DownLoad, GetInfo, SendCommand — 实现在 RoomInterface.cpp

int CRoom::handle_action(char* comm, CChar* me, char* arg, char* tar)
{
    if (strcmp(comm, "back") == 0) {
        char msg[80];
        // 检查是否返回总部
        CMapping* ptr = querymap("corps");
        if (!ptr) return 0;

        CVector v;
        ptr->keys(v);

        const char* corps = me->querystr("corps/id");

        if (v.find(corps) == v.end()) return 0;

        message_vision("$N左右看看，突然矮身一跃，便消失了影踪。", me);
        me->move(load_room(corps));
        tell_room((me->environment()),
                  snprintf(msg, 80, "%s走了进来。", me->name()), me);
        return 1;
    }

    return 0;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

int l_draw_map(lua_State* L)
{
    draw_map();
    return 0;
}

// self:remove_all_doors() - 移除房间全部出口（CRoom）
int l_remove_all_doors(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CRoom* room = dynamic_cast<CRoom*>(obj);
    if (!room) return 0;
    room->remove_all_doors();
    return 0;
}

// self:remove_all_box() - 移除房间全部箱子及其内容（CRoom）
int l_remove_all_box(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CRoom* room = dynamic_cast<CRoom*>(obj);
    if (!room) return 0;
    room->remove_all_box();
    return 0;
}

// self:reset() - 重置房间（CRoom）
int l_room_reset(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CRoom* room = dynamic_cast<CRoom*>(obj);
    if (!room) return 0;
    room->reset();
    return 0;
}

// self:remove_doors(next) - 移除通往 next 房间的出口（CRoom）
int l_remove_doors(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CRoom* room = dynamic_cast<CRoom*>(obj);
    if (!room) return 0;
    const char* next = luaL_checkstring(L, 2);
    room->remove_doors(next);
    return 0;
}

// self:add_npc(base_name, bx, by, ex, ey, dir, active, ival) - 往房间增加NPC
int l_add_npc(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CRoom* r = dynamic_cast<CRoom*>(obj);
    if (!r) return 0;

    const char* base_name = luaL_checkstring(L, 2);
    int32_t bx            = (int32_t)luaL_optinteger(L, 3, 0);
    int32_t by            = (int32_t)luaL_optinteger(L, 4, 0);
    int32_t ex            = (int32_t)luaL_optinteger(L, 5, 0);
    int32_t ey            = (int32_t)luaL_optinteger(L, 6, 0);
    int dir               = (int)luaL_optinteger(L, 7, 0);
    int active            = (int)luaL_optinteger(L, 8, 0);
    int ival              = (int)luaL_optinteger(L, 9, 0);

    // 单个转换，无静态缓冲区冲突
    CNpc* npc = r->add_npc(base_name, bx, by, ex, ey, dir, active, ival);

    // 返回创建的 NPC 对象（供脚本 set_name / init 等后续操作）
    lua_bind_push_object(L, npc);
    return 1;
}

// self:add_door(door, next_room, next_door, dir) -> object -
// 增加出口，返回出口对象
int l_add_door(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    CRoom* r         = obj ? dynamic_cast<CRoom*>(obj) : NULL;
    if (!r) {
        lua_pushnil(L);
        return 1;
    }

    const char* door      = luaL_checkstring(L, 2);
    const char* next_room = luaL_checkstring(L, 3);
    const char* next_door = luaL_optstring(L, 4, "");
    const char* dir       = luaL_optstring(L, 5, "");

    // 多次调用必须复制到局部变量
    char doorBuf[256], nextBuf[256], nextDoorBuf[256], dirBuf[64];
    strncpy(doorBuf, door, 255);
    doorBuf[255] = 0;
    strncpy(nextBuf, next_room, 255);
    nextBuf[255] = 0;
    strncpy(nextDoorBuf, next_door, 255);
    nextDoorBuf[255] = 0;
    strncpy(dirBuf, dir, 63);
    dirBuf[63] = 0;

    lua_bind_push_object(L, r->add_door(doorBuf, nextBuf, nextDoorBuf, dirBuf));
    return 1;
}

// self:add_item(item_name) -> object - 往房间放物品
int l_add_item(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CRoom* r = dynamic_cast<CRoom*>(obj);
    if (!r) return 0;

    const char* item_name = luaL_checkstring(L, 2);
    lua_bind_push_object(L, r->add_item(item_name));
    return 1;
}

// self:Add_Box(box_type) -> object - 往房间放箱子（默认 "box"）
int l_add_box(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CRoom* r = dynamic_cast<CRoom*>(obj);
    if (!r) return 0;

    const char* box_type = luaL_optstring(L, 2, "box");
    lua_bind_push_object(L, r->add_box(box_type));
    return 1;
}

// self:PresentDoor(id) -> object - 取出口对象（id 或方向均可），无则为 nil
int l_present_door(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    CRoom* r         = obj ? dynamic_cast<CRoom*>(obj) : NULL;
    if (!r) {
        lua_pushnil(L);
        return 1;
    }

    const char* id = luaL_checkstring(L, 2);
    lua_bind_push_object(L, r->present_door(id));
    return 1;
}

// room_exits(room) -> {{id, next_door}, ...} - 房间出口列表（寻宝怪 search 用）
int l_room_exits(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CRoom* r = dynamic_cast<CRoom*>(obj);
    if (!r) return 0;

    DTEXITLIST* list = r->query_exits();
    if (!list) {
        lua_pushnil(L);
        return 1;
    }

    int n = (int)list->size();
    if (n <= 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, n, 0);
    int idx = 1;
    for (auto door : *list) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, door->querystr("id") ? door->querystr("id") : "");
        lua_setfield(L, -2, "id");
        lua_pushstring(
            L, door->querystr("next_door") ? door->querystr("next_door") : "");
        lua_setfield(L, -2, "next_door");
        lua_rawseti(L, -2, idx++);
    }
    return 1;
}

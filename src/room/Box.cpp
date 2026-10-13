/** @file Box.cpp @brief implementation of the CBox class. */

#include <windows.h>

#include "Box.h"
#include "Global.h"

#include "../db/Sql.h"

extern bool g_reboot;

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
void CBox::locked(char* reason, int32_t duration, CChar* me)
{
    set("status", "locked");

    set("reason", reason);
    set("duration", duration);                 // 耐久度
    if (me) set("owner", me->querystr("id"));  // 上锁的主人

    update_me();
};

CContainer* CBox::add_object(const char* obj_name, int32_t amount)
{
    int i, j, k;

    CContainer* item = g_item_mgr->load_item(obj_name);  // 载入物件

    if (item->query_attr(CAN_COMBINE)) {
        j = 1;
        k = amount;
    }
    else {
        j = amount;
        k = 1;
    }

    for (i = 0; i < j; i++) {
        if (item == NULL) {
            item = g_item_mgr->load_item(obj_name);  // 载入物件
        }
        if (k > 1) item->set_amount(k);

        item->move(this);
        item->set("no_drop_man", 1);  // 箱子里不可以装人

        item = NULL;
    }

    return item;
}

void CBox::download(CChar* me)
{
    CItem::download(me);

    if (EQUALSTR(querystr("status"), "locked"))
        return;  // 上锁的箱子看不到内部物品。

    for (auto item : m_item_list) {
        me->on_add_object(item, this);
    }
}

// 处理动作
int CBox::handle_action(char* comm, CChar* me, char* arg)
{
    if (strcmp(comm, "open") == 0) {
        // 如果箱子是某人封上的，钥匙无法打开。
        if (querystr("owner")[0]
            && DIFFERSTR(querystr("owner"), me->querystr("id"))) {
            message_vision_room(me, "tell", "你无法打开。");
            return 0;
        }

        // 检查me身上是否有钥匙
        DTItemList* t = me->get_item_list();

        for (auto key : *t) {
            if (EQUALSTR(key->querystr("base_name"), "key")) {
                // 试开
                if (key->query("password") == query("password")) {
                    char msg[255];

                    un_locked();
                    snprintf(msg, 255, "只听咔的一响，%s用手中的%s打开了%s。",
                             me->name(), key->name(), name());
                    message_vision_room(me, "chat", msg);
                    return 1;
                }
            }
        }

        message_vision_room(me, "tell", "你无法打开。");
    }

    return 0;
}

void CBox::on_enter(CContainer* me)
{
    CContainer::on_enter(me);

    m_update++;

    // 通知同一环境内玩家，增加了物件。
    DTItemList* t;

    if (!m_environment) {
        LOG_ERROR("当%s移入%s时，发现箱子没有环境！", me->name(), name());
        return;
    }

    t = m_environment->get_item_list();
    for (auto obj : *t) {
        if (obj->query_attr(IS_USER)) obj->on_add_object(me, this);
    }

    // 存盘
    if (m_environment->query("saveobjects")) save_object();
}

void CBox::on_leave(CContainer* me)
{
    CContainer::on_leave(me);

    m_update++;

    if (!m_environment) return;  // 当Destruct时,是没有环境的。

    // 通知同一环境内玩家，增加了物件。
    DTItemList* t;

    t = environment()->get_item_list();
    for (auto obj : *t) {
        if (obj->query_attr(IS_USER)) obj->on_rem_object(me, this);
    }

    if (m_environment->query("saveobjects")
        && !g_reboot)  // 当重起时,销毁物件移出不算.
        save_object();
}

int CBox::save_local()
{
    if (m_destruct) return 1;
    // CContainer::Save();
    if (m_environment->query("saveobjects")
        && !g_reboot)  // 当重起时,销毁物件移出不算.
        save_object();
    DTItemList* t;

    t = environment()->get_item_list();
    for (auto obj : *t) {
        if (obj->query_attr(IS_USER)) download((CChar*)obj);
    }
    return 1;
}

int CBox::load_object()
{
    CMapping m;
    int count = 0;
    char msg[20];

    if (!m_environment || !m_environment->query("saveobjects")) return 0;

    // 从数据库加载箱子内容
    std::string data;
    if (!db_load_container("t_box", querystr("db_room_id"), data)) {
        LOG_ERROR("CBox::LoadObject: db_load_container failed for %s",
                  querystr("db_room_id"));
        return 0;
    }

    if (!m.load_buffer(data.c_str())) {
        LOG_ERROR("CBox::LoadObject: LoadBuffer failed for %s",
                  querystr("db_room_id"));
        return 0;
    }

    count = m.query("count");

    for (int i = 0; i < count; i++) {
        CMapping* ptr = m.querymap(snprintf(msg, 20, "%d", i));

        if (!ptr) return 0;

        CContainer* obj = g_item_mgr->load_item(ptr->querystr("name"),
                                                ptr->querystr("unique"));
        if (!obj) {
            LOG_ERROR("CBox::LoadObject: 物件%s创建失败!",
                      ptr->querystr("name"));
            return 0;
        }
        obj->create(ptr->query("index"));
        obj->setup();
        obj->set_amount(ptr->query(
            "amount"));  // 数量无条件恢复(箱子存盘恒写amount),并重算weight/value
        obj->move(this);
    }

    return 1;
}

int CBox::save_object()
{
    CMapping m;
    char msg[80];

    if (!m_environment || !m_environment->query("saveobjects")) return 0;

    // 将箱子内的物件存盘
    int count = 0;

    for (auto obj : m_item_list) {
        if (obj->is_character()) continue;
        if (obj->query_attr(IS_MONEY)) continue;  // 钱不能放到箱子里存盘

        m.set(snprintf(msg, 80, "%d/name", count), obj->querystr("base_name"));
        m.set(snprintf(msg, 80, "%d/index", count), obj->query("index"));
        m.set(snprintf(msg, 80, "%d/amount", count), obj->query_amount());
        m.set(snprintf(msg, 80, "%d/unique", count),
              obj->querystr("unique_id"));

        count++;
    }

    m.set("count", count);

    // 保存到数据库 t_box (使用通用容器接口)
    // 先检查是否存在以决定是 insert 还是 update
    std::string existing;
    bool is_update
        = db_load_container("t_box", querystr("db_room_id"), existing);
    if (!db_save_container("t_box", querystr("db_room_id"), name(1),
                           m.get_buffer(), is_update)) {
        LOG_ERROR("CBox::SaveObject: db_save_container failed for %s",
                  querystr("db_room_id"));
    }
    return 1;
}

void CBox::remove_objects()
{
    // 从数据库删除箱子内容 (使用通用容器接口)
    if (!db_delete_container("t_box", querystr("db_room_id"))) {
        LOG_ERROR("CBox::RemoveObjects: db_delete_container failed for %s",
                  querystr("db_room_id"));
    }
}

CBox::~CBox()
{
    if (m_environment && !m_environment->query("saveobjects")) {
        // 将箱子内的物件销毁
        // 先收集待销毁物件，再统一销毁（避免在遍历 m_ItemList 时调用
        // DESTRUCT_OB 导致迭代器失效）
        DTItemList toDestruct;
        for (auto obj : m_item_list) {
            // 声明了专属存盘表的物件（如套装）：随箱子消失一并删除存档行
            if (obj->querystr("save_table")[0]) {
                obj->purge();
                toDestruct.push_back(obj);
            }
        }
        for (auto obj : toDestruct) {
            DESTRUCT_OB(obj, "~CBox()");
        }
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:UnLocked([resume]) -> number - 开锁（CBox；resume>0 时扣减耐久）
int l_unlocked(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CBox* box = dynamic_cast<CBox*>(obj);
    if (!box) return 0;

    int32_t resume = (int32_t)luaL_optinteger(L, 2, 0);
    lua_pushinteger(L, box->un_locked(resume));
    return 1;
}

// self:Locked(season, duration[, me]) - 上锁（CBox）
int l_locked(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CBox* box = dynamic_cast<CBox*>(obj);
    if (!box) return 0;

    const char* season = luaL_checkstring(L, 2);
    int32_t duration   = (int32_t)luaL_checkinteger(L, 3);
    CChar* me          = NULL;
    if (lua_isuserdata(L, 4)) {
        CMainObject* meObj = get_object(L, 4);
        if (meObj) me = dynamic_cast<CChar*>(meObj);
    }
    box->locked((char*)season, duration, me);
    return 0;
}

// self:add_object(obj_name, nAmount) -> object - 往箱子放东西（CBox）
int l_add_object(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CBox* b = dynamic_cast<CBox*>(obj);
    if (!b) return 0;

    const char* obj_name = luaL_checkstring(L, 2);
    int32_t amount       = (int32_t)luaL_optinteger(L, 3, 1);

    lua_bind_push_object(L, b->add_object(obj_name, amount));
    return 1;
}

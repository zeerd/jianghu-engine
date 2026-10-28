/**
 * @file ContainerMove.cpp
 * @brief CContainer: movement, environment,
 * enter/leave, actions.  */

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

// 从一个容器移动到另一个容器。
// 此间有一个很复杂的通讯指令
int CContainer::move(CContainer* me)
{
    CContainer* env;

    char env_old[40] = "(NULL)";
    char env_new[40] = "(NULL)";
    if (environment()) strncpy(env_old, environment()->name(1), 30)[30] = 0;
    if (me) snprintf(env_new, 40, "%s(%s)", me->name(1), me->id(1));

    // 如果不是能结合的物品数量超过1，移动到容器中，保留证据//自动消失
    if (me && (query_amount() > 1) && !query_attr(CAN_COMBINE)) {
        LOG_ERROR("物件%s移入失败：涉嫌复制，复制人%s。", name(), me->name(1));
        // DESTRUCT_OB(this, "CContainer::move");
        set("数目", query_amount());
        if (environment()) set("复制人来源", environment()->id(1));
        if (me) set("复制人", me->id(1));
        set_amount(1);
        (void)move(load_room("pk"));
        return 0;
    }

    if (query("value") >= 500000 && is_user(me)) {
        char msg[255];
        char color[10];

        color[0] = 0;
        if (query("value") >= 5000000) strcpy(color, "$HIR");

        g_channel.do_channel(
            &g_channel, 0, "sys",
            snprintf(msg, 255, "%s价值%ld的%s从%s移动到%s。", color,
                     query("value"), name(1), env_old, env_new));
        LOG_WARN("%s", msg);
    }

    if (query("equipped")) {
        auto* handler = dynamic_cast<IActionHandler*>(this);
        if (handler && !handler->do_action(CChar::DO_UNWIELD)) {
            LOG_WARN("物件%s移入失败：解除装备失败。", name());
        }
    }

    // 向NULL和标准房间中移动必须成功，否则在销毁物件时，会出现内存访问错误。
    if (me) {
        if (me == this) {
            LOG_DEBUG("物件%s移入%s失败：不能自己放自己", name(), me->name());
            return 0;  // 不能自己放自己
        }

        env = this;
        while ((env = env->environment()))
            if (env == me) break;

        if (!env) {
            if (!me->query("infinity")
                && me->query_encumb() + weight()
                       > me->query_max_encumbrance()) {
                LOG_DEBUG("物件%s移入%s失败：太重了。重：%ld 负重: %ld / %ld",
                          name(), me->name(), weight(), me->query_encumb(),
                          me->query_max_encumbrance());
                // 为了预防玩家不停的移动造成复制，在移动失败后强制忙乱
                if (is_user(me)) ((CChar*)me)->start_busy(4);
                return 0;  // 太重了。
            }
        }
    }

    // Move_Object:
    if (m_environment) {
        m_environment->on_leave(this);

        if (is_user(m_environment)) m_environment->save();

        m_environment = NULL;
    }

    if (me)  // 进入新环境
    {
        m_environment = me;

        // 对于移动者本身而言，是切换场景
        me->change_room(this);

        // 未切换场景直接进入
        if (!query_temp("client/loading")) me->on_enter(this);
    }
    else
        m_environment = NULL;

    on_moved(me);
    return 1;
}

// 被成功的移入了me中
void CContainer::on_moved(CContainer* me) {}

CContainer* CContainer::environment() { return m_environment; }

void CContainer::change_room(CContainer* me)
{
    char cmd[512];

    if (std::find(m_item_list.begin(), m_item_list.end(), me)
        == m_item_list
               .end()) {  // 如果已经存在则不必增加了。如登录时的重新连线呼叫。
        add_encumbrance(me->weight());
        m_item_list.push_front(me);  // 增加
        on_add_object(me, this);     // 对场景而言,是加了个东西
    }

    if (!is_user(me)) return;

    const char* map = querystr("map");

    // 切换场景之对玩家发
    snprintf(cmd, 480, "&C=%d&R=%d&I=%s&N=%s&F=%d&D=%s&", GOTOROOM, object_id(),
             map[0] ? map : "玩家民居1", name(),
             query_attr(CChar::IS_FIGHTROOM), me->querystr_temp("init_door"));

    static_cast<CUser*>(me)->send_command(cmd, SEND_WAIT_BUFFERED);
    // 用户要等待切换场景
    me->set_temp("client/loading", 1);

    snprintf(cmd, 255, "&C=%d&R=0&",
             ADDDOOR);  // 将传出口信息标志，告诉客户端切换场景完毕。
    static_cast<CUser*>(me)->send_command(cmd, SEND_WAIT_BUFFERED);

    // 如果是非房间物件，要求sendoff，不然不能及时刷新
    if (!query_attr(IS_ROOM)) static_cast<CUser*>(me)->send_off();
}

// 档案从数据库彻底删除
void CContainer::remove()
{
    if (!query_attr(CAN_SAVE)) return;

    std::string table_name, obj_id;
    save_route(table_name, obj_id);

    if (!db_delete_container(table_name.c_str(), obj_id.c_str())) {
        LOG_ERROR("removeobject:query db fail!");
    }
}

int CContainer::visible()
{
    if (query_temp("client/loading")) return 0;
    return 1;
}

// 收集本容器内未屏蔽该频道的玩家
MAP2USER* CContainer::get_users(const char* channel, MAP2USER* list)
{
    if (!this) return list;

    for (auto obj2 : m_item_list) {
        if (is_user(obj2)
            && !static_cast<CUser*>(obj2)->refuse_channel(channel))
            (*list)[obj2->object_id()] = static_cast<CUser*>(obj2);
    }

    return list;
}

// 离开场景
void CContainer::on_leave(CContainer* me)
{
    auto it = std::find(m_item_list.begin(), m_item_list.end(), me);
    if (it == m_item_list.end()) return;

    add_encumbrance(-me->weight());
    m_item_list.erase(it);

    on_rem_object(me, this);  // 对场景而言,是少了个东西

    for (auto obj : m_item_list) {
        if (obj == me) continue;
        if (obj->query_attr(IS_USER))
            obj->on_rem_object(me, this);  // 对其他人,是我走了.
    }
}

// 进入场景
void CContainer::on_enter(CContainer* me)
{
    DTItemList search;

    copy_list(&search, &m_item_list);

    for (auto obj : search) {
        if (obj == me) continue;
        if (obj->environment() != this) continue;         // 此玩家已经移动了。
        if (obj->query_temp("client/loading")) continue;  // 该物件尚未进入地图

        if (obj->query_attr(IS_USER))
            obj->on_add_object(me, this);  // 对其他人,是我进来了.

        if (me->query_attr(IS_USER)) {
            me->on_add_object(obj, this);  // 对我来说,是其他人进来了.

            if (obj->is_character())  // 角色在人物进入房间的时候初始化
            {
                obj->init(static_cast<CChar*>(me));

                // 如人物在init时被转移.则不再执行下面的init
                if (me->environment() != this) return;
            }
        }
    }

    search.clear();

    // 房间初始化
    if (me->query_attr(IS_CHAR))
        init(static_cast<CChar*>(me));
    else if (me->query_attr(IS_ITEM))
        static_cast<CItem*>(me)->init();
}

// 增加一个物件的通讯
void CContainer::on_add_object(CContainer* obj, CContainer* env)
{
    if (!is_user(this)) return;

    char tmp[64];
    std::string cmd;
    cmd += snprintf(tmp, static_cast<int>(sizeof(tmp)), "&C=%d&E=%d&T=%d&U=%d&",
                    ADDOBJECT, env->object_id(), obj->object_id(),
                    obj->query_attr(CChar::UPDATE));

    if (obj->query_attr(IS_CHAR))  // 接上状态描述
        static_cast<CChar*>(obj)->get_state_msg(cmd);

    if (obj->query_attr(IS_ITEM) || obj->query_attr(IS_CORPSE)) cmd += "i=1&";
    if (obj->query_attr(IS_USER)) cmd += "u=1&";

    if (auto* cli = dynamic_cast<IClientObject*>(this))
        cli->send_command(cmd.c_str(), SEND_WAIT_BUFFERED);

    // 再发物件详细资料
    if (auto* cli = dynamic_cast<IClientObject*>(obj))
        cli->download(static_cast<CChar*>(this));

    // 再发物件位置
    static_cast<CUser*>(this)->location_object(obj);
}

// 移除一个物件的通讯
void CContainer::on_rem_object(CContainer* obj, CContainer* env)
{
    char cmd[480];

    snprintf(cmd, 480, "&C=%d&E=%d&T=%d&", REMOVEOBJECT, env->object_id(),
             obj->object_id());

    if (auto* cli = dynamic_cast<IClientObject*>(this))
        cli->send_command(cmd, SEND_WAIT_BUFFERED);
}

// 名字/id 变化后的重发：协议没有「改名」消息，名字只在物件进场景时下发，
// 因此复用「进入场景」的 ADDOBJECT + DOWNOBJECT 组合，交给同一容器内的
// 在线玩家。客户端按 object id 幂等合并，重复包不会产生重复条目。
void CContainer::on_name_changed()
{
    if (!m_environment) return;

    // 物品在玩家身上/背包：容器本身就是持有者
    if (is_user(m_environment)) {
        m_environment->on_add_object(this, m_environment);
        return;
    }

    DTItemList search;
    copy_list(&search, m_environment->get_item_list());
    for (auto obj : search) {
        if (is_user(obj)) obj->on_add_object(this, m_environment);
    }
    search.clear();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"

// self:move(target)
int l_move(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    CContainer* target = NULL;
    if (lua_isuserdata(L, 2))
        target = dynamic_cast<CContainer*>(get_object(L, 2));

    if (target) c->move(target);
    return 0;
}

// self:environment() -> object
int l_environment(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    lua_bind_push_object(L, c->environment());
    return 1;
}

// self:remove() - 删除容器的持久化档案（CContainer::remove）
int l_remove(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CContainer* c = dynamic_cast<CContainer*>(obj);
    if (!c) return 0;

    c->remove();
    return 0;
}

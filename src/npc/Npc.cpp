/**
 * @file Npc.cpp
 * @brief implementation of the CNpc class. */

#include <windows.h>

#include <algorithm>

#include "Npc.h"

#include "Global.h"
#include "FightRoom.h"
#include "Room.h"
#include "LuaHook.h"

//---------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------
CNpc::~CNpc() { m_list_inquiry.clear(); }

int32_t CNpc::query_attr(int attr)
{
    if (attr == IS_NPC) return 1;
    if (attr == CAN_SAVE) return query("can_save");

    return CChar::query_attr(attr);
}

// 随机行动
int CNpc::self_action()
{
    int chance;
    const char* ptr = NULL;

    if (!environment()) return 0;
    if (!living(this)) return 0;

    if (!(chance
          = is_fighting() ? query("chat_chance_combat") : query("chat_chance")))
        return 0;

    if (g_utils.random(100) < chance) {
        if (is_fighting()) {
            // 判断一下是否在战斗room
            if (environment()->query_attr(IS_FIGHTROOM))
                ptr = chat_msg_combat();
            else
                LOG_WARN("NPC%s在环境%s中出于战斗状态！", name(),
                         environment()->name());
        }
        else
            ptr = chat_msg();

        if (ptr && ptr[0]) message_vision_room(this, "chat", ptr);

        return 1;
    }

    return 0;
}

char* CNpc::chat_msg() { return 0; }

char* CNpc::chat_msg_combat() { return 0; }

// 回到该回的地方去
int CNpc::return_home(CContainer* home)
{
    if (!environment() || environment() == home) return 1;

    if (!query_attr(IS_LIVING)) return 0;  // no return
    double value = 900;                    // 超过5分钟战斗的NPC自动回去
    time_t t;
    time(&t);
    t -= (time_t)value;

    if (is_fighting() && (time_t)query_temp("fight/join_time") > t)
        return 0;  // is fighting

    // Leave for home now.
    message_vision("$N匆匆忙忙离开了。", this);
    move(home);
    return 1;
}

void CNpc::create() {}

// 携带物件
CContainer* CNpc::carry_object(const char* obj_name, int32_t amount, int idx)
{
    if (amount <= 0) amount = 1;

    CContainer* item = g_item_mgr->load_item(obj_name);
    if (idx) item->create(idx);

    if (item->query_attr(CAN_COMBINE)) {
        // 可堆叠物品：设置数量后直接移入
        item->set_amount(amount);
        item->move(this);
    }
    else {
        // 不可堆叠物品：逐个创建独立实例
        item->set_amount(1);
        item->move(this);
        for (int32_t i = 1; i < amount; i++) {
            CContainer* extra = g_item_mgr->load_item(obj_name);
            if (idx) extra->create(idx);
            extra->set_amount(1);
            extra->move(this);
        }
    }
    return item;
}

CNpc* random_npc();

int CNpc::accept_object(CChar* me, CContainer* item)
{
    // 通用收物钩子：脚本侧（任务等）接管本次物品交付。
    // 钩子缺席 = 不适用，按引擎默认（不接收）处理。
    CLuaHookCall hook("pre_accept_object");
    if (hook.valid()) {
        hook.add_object(this).add_object(me).add_object(item);
        if (hook.call()) {
            bool handled = false;
            hook.table_field_bool("handled", handled);
            if (handled) return 1;
        }
    }
    return 0;
}

// 给NPC增加金钱
void CNpc::add_money(int32_t amount)
{
    CContainer* money;

    if (!amount) return;
    money = g_item_mgr->load_item("coin");
    money->set_amount(amount);
    money->move(this);
}

// 设置查询问题
void CNpc::set_inquiry(const char* ask, const char* answer, const char* topic)
{
    auto p = std::make_unique<Inquiry_t>();

    if (topic) p->topic = topic;
    p->answer           = answer;
    p->func             = NULL;
    m_list_inquiry[ask] = std::move(p);
}

void CNpc::set_inquiry(const char* ask, INQUERYFUNC func, const char* topic)
{
    auto p = std::make_unique<Inquiry_t>();

    if (topic) p->topic = topic;
    p->func             = func;
    m_list_inquiry[ask] = std::move(p);
}

// 打招呼的函式
char* CNpc::greeting(CChar* me) { return ""; }

// learn用到
int CNpc::recognize_apprentice(CChar* me) { return 0; }

// 通用对话钩子（hooks.pre_talk）：脚本侧（任务等）是否接管本次对话。
// 钩子缺席 = 不适用；钩子返回 table 时读 handled / flow 两个字段：
//   handled = true  → 接管本次对话
//   flow = "engine" → 不接管，但本次对话归引擎标准询问流程
TalkHookResult query_talk_hook(CNpc* npc, CChar* me, char* ask)
{
    TalkHookResult result;
    if (!npc || !me || !ask) return result;

    CLuaHookCall hook("pre_talk");
    if (!hook.valid()) return result;
    hook.add_object(npc).add_object(me).add_str(ask);
    if (!hook.call()) return result;

    hook.table_field_bool("handled", result.handled);
    std::string flow;
    if (hook.table_field_str("flow", flow) && flow == "engine")
        result.engine_flow = true;
    return result;
}

// 与NPC谈话
int CNpc::do_talk(CChar* me, char* ask)
{
    if (!me || !ask) return 0;

    // 通用对话钩子：脚本侧接管本次对话则终止对话，不再走询问菜单。
    TalkHookResult hook_result = query_talk_hook(this, me, ask);
    if (hook_result.handled) return 1;

    return do_talk_inquiry(me, ask);
}

// 引擎标准询问流程（C++ 询问菜单）：不含通用对话钩子。
int CNpc::do_talk_inquiry(CChar* me, char* ask)
{
    string msg;
    Inquiry_t* inquiry;

    if (!me || !ask) return 0;

    // 查询ask
    if (m_list_inquiry.find(ask) == m_list_inquiry.end()) {
        if (!m_list_inquiry.size()) {
            return notify_fail("对方并不愿意理你。");
        }

        LISTINQUIRY::iterator p;
        for (p = m_list_inquiry.begin(); p != m_list_inquiry.end(); p++) {
            inquiry = p->second.get();
            if (inquiry->topic.length())
                add_menu_item(inquiry->topic.c_str(), p->first.c_str(), me);
            else {
                msg = "向";
                msg += name();
                msg += "请教关于";
                msg += p->first;
                msg += "的问题。";
                add_menu_item(msg.c_str(), p->first.c_str(), me);
            }
        }
        send_menu(me);
        return 1;
    }

    inquiry = m_list_inquiry[ask].get();

    if (inquiry->func) {
        const char* rtn;

        rtn = (*inquiry->func)(this, me);

        if (rtn && rtn[0]) {
            say(rtn, me);
            send_menu(me);
        }
    }
    else {
        say(inquiry->answer.c_str(), me);
        send_menu(me);
    }

    return 1;
}

void CNpc::random_move()
{
    if (!m_environment->query_attr(IS_ROOM)) return;

    CRoom* room      = (CRoom*)m_environment;
    DTEXITLIST* list = room->query_exits();

    int doors = list->size();
    if (doors) {
        char msg[80];
        auto it = list->begin();
        std::advance(it, g_utils.random(doors));
        CDoor* dir = *it;
        command(snprintf(msg, 80, "go 0 %s", dir->querystr("id")));
    }
}

// 使用cast target = 0:自己 1:敌人
int CNpc::perform_action(char* magic, int target)
{
    char msg[255];
    CFightRoom* env;
    CChar* who = this;

    if (target && environment()->query_attr(IS_FIGHTROOM)) {
        env = (CFightRoom*)environment();
        who = env->select_member(1 - query_team());
        if (!who) return 0;
    }

    return command(snprintf(msg, 255, "cast %ld %s", who->object_id(), magic));
}

int CNpc::accept_fight(CChar* me)
{
    char msg[255];
    string attr = querystr("attitude");

    if (query("hp") * 100 / query("max_hp") >= 90) {
        if (attr == "friendly") {
            say(snprintf(msg, 255, "%s怎么可能是%s的对手？", query_self(this),
                         query_respect(me)),
                me);
            send_menu(me);
            return 0;
        }
        else if (attr == "aggressive" || attr == "killer") {
            say("哼！出招吧！");
        }
        else if (!is_fighting()) {
            say(snprintf(msg, 255, "既然%s赐教，%s只好奉陪。",
                         query_respect(me), query_self(this)));
        }
        set("hp", query("max_hp"));
        set("eff_hp", query("max_hp"));
        set("mp", query("max_mp"));
        return 1;
    }

    return 0;
}

void CNpc::wield_ownered_item(const char* name)
{
    CContainer* obj = carry_object(name);
    obj->set("owner", querystr("base_name"));
    obj->set("no_get", "这不是你的东西。");
    obj->set("no_duo", 1);
    if (auto* eq = dynamic_cast<IEquippable*>(obj)) eq->wield();
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:carry_object(name) -> object
int l_carry_object(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CNpc* npc = dynamic_cast<CNpc*>(obj);
    if (!npc) return 0;

    const char* name = luaL_checkstring(L, 2);
    int32_t amount   = (int32_t)luaL_optinteger(L, 3, 1);
    lua_bind_push_object(L, npc->carry_object(name, amount));
    return 1;
}

// self:wield_ownered_item(name) -
// 携带并装备一件属于自己（owner=base_name）的武器
int l_wield_ownered_item(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CNpc* npc = dynamic_cast<CNpc*>(obj);
    if (!npc) return 0;

    const char* name = luaL_checkstring(L, 2);
    npc->wield_ownered_item(name);
    return 0;
}

// self:perform_action(magic, target)
int l_perform_action(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;

    CNpc* npc = dynamic_cast<CNpc*>(obj);
    if (!npc) return 0;

    const char* magic = luaL_checkstring(L, 2);
    int target        = (int)luaL_optinteger(L, 3, 1);
    npc->perform_action((char*)magic, target);
    return 0;
}

// self:set_inquiry(ask, answer, topic) - 预置回答
int l_set_inquiry(lua_State* L)
{
    CMainObject* obj   = get_object(L, 1);
    const char* ask    = luaL_checkstring(L, 2);
    const char* answer = luaL_checkstring(L, 3);
    const char* topic  = lua_isstring(L, 4) ? lua_tostring(L, 4) : NULL;
    char askBuf[256], ansBuf[512], topBuf[256];
    strncpy(askBuf, ask, sizeof(askBuf) - 1);
    askBuf[sizeof(askBuf) - 1] = 0;
    strncpy(ansBuf, answer, sizeof(ansBuf) - 1);
    ansBuf[sizeof(ansBuf) - 1] = 0;
    if (topic) {
        strncpy(topBuf, topic, sizeof(topBuf) - 1);
        topBuf[sizeof(topBuf) - 1] = 0;
    }
    if (CLuaNpc* npc = dynamic_cast<CLuaNpc*>(obj)) {
        npc->set_inquiry(askBuf, ansBuf, topic ? topBuf : NULL);
    }
    else if (CLuaMirrorNpc* npc = dynamic_cast<CLuaMirrorNpc*>(obj)) {
        npc->set_inquiry(askBuf, ansBuf, topic ? topBuf : NULL);
    }
    return 0;
}

// self:set_inquiry_func(ask, funcName, topic) - 回调版，funcName 为脚本 table
// 中的函数名
int l_set_inquiry_func(lua_State* L)
{
    CMainObject* obj     = get_object(L, 1);
    const char* ask      = luaL_checkstring(L, 2);
    const char* funcName = luaL_checkstring(L, 3);
    const char* topic    = lua_isstring(L, 4) ? lua_tostring(L, 4) : NULL;
    char askBuf[256], topBuf[256];
    strncpy(askBuf, ask, sizeof(askBuf) - 1);
    askBuf[sizeof(askBuf) - 1] = 0;
    if (topic) {
        strncpy(topBuf, topic, sizeof(topBuf) - 1);
        topBuf[sizeof(topBuf) - 1] = 0;
    }
    if (CLuaNpc* npc = dynamic_cast<CLuaNpc*>(obj)) {
        npc->set_inquiry_func(askBuf, funcName, topic ? topBuf : NULL);
    }
    else if (CLuaMirrorNpc* npc = dynamic_cast<CLuaMirrorNpc*>(obj)) {
        npc->set_inquiry_func(askBuf, funcName, topic ? topBuf : NULL);
    }
    return 0;
}

// self:random_move()
int l_random_move(lua_State* L)
{
    CNpc* n = dynamic_cast<CNpc*>(get_object(L, 1));
    if (!n) return 0;
    n->random_move();
    return 0;
}

// self:add_money(amount) - 给 NPC 加钱
int l_add_money(lua_State* L)
{
    CNpc* n = dynamic_cast<CNpc*>(get_object(L, 1));
    if (!n) return 0;
    int32_t amount = (int32_t)luaL_checkinteger(L, 2);
    n->add_money(amount);
    return 0;
}

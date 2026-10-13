// test_lua_error_paths.cpp — 脚本回调抛错时的报错与回退契约
//
// 契约：CLuaNpcT / CLuaItemT / CLuaRoomT / CLuaSkillT 的每个脚本回调包装层、
// 以及 LuaQuest 的四个事件钩子，在 lua_pcall 失败时必须
//   1. 打一条 ERROR 日志，指明是哪个脚本的哪个回调失败（调用方定位线上
//      故障的唯一线索：格式串 + 脚本名 + Lua 错误原文）；
//   2. 按该回调既定语义回退基类实现（或静默吞掉），绝不让 Lua 错误穿透
//      到 C++ 调用方。
// 脚本加载/重载的三态（语法错 / 运行错 / 未返回 table）同样归本文件所有：
// 旧版本必须保留可用，且三种失败各打一条不同的日志。
//
// 夹具：tests/lua-tests/{npc,item,room,skill,quest}/ 下的 t_error_* 与
// quest_mgr，每个回调都 error()，一条用例即可驱动整个报错面。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <windows.h>

#include <cstdio>
#include <memory>
#include <string>

#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "Item.h"
#include "ItemMgr.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "RoomMgr.h"
#include "Skill.h"
#include "SkillMgr.h"
#include "LuaEngine.h"
#include "LuaItem.h"
#include "LuaNpc.h"
#include "LuaQuest.h"
#include "LuaRoom.h"
#include "LuaSkill.h"
#include "mock/mock_itemmgr.h"
#include "test_log_hits.h"
#include "test_lua_env.h"

extern std::unique_ptr<CContainer> create_item(string base_name,
                                               string unique_id);
extern int lua_reload_script(const char* name);

namespace {

// 报错日志探针：构造时快照 needle 的命中数，ExpectOnce 断言恰好新增一次。
// needle 取自报错格式串（含脚本名），与日志行首前缀无关——前缀会随
// 封装层调整而变，报错内容才是被守护的契约。
class ErrorLogProbe {
public:
    explicit ErrorLogProbe(const char* needle)
        : m_needle(needle), m_before(count_log_hits(needle))
    {
    }

    void ExpectOnce() const
    {
        EXPECT_EQ(count_log_hits(m_needle), m_before + 1) << m_needle;
    }

private:
    const char* m_needle;
    int m_before;
};

// 把对象从世界脱离：停心跳 + 离开所在环境（技能没有环境概念，不挂此回调）
template <typename T>
void detach_from_world(T* ob)
{
    if (!ob) return;
    ob->set_heart_beat(0);
    ob->move(nullptr);
}

// 用例退出时（含 ASSERT 提前返回）把对象从全局表摘除并销毁。
template <typename Map>
class ScopedGlobalEntry {
public:
    using DetachFn = void (*)(typename Map::mapped_type);

    ScopedGlobalEntry(Map& map, typename Map::key_type key,
                      DetachFn detach = nullptr)
        : m_map(map), m_key(std::move(key)), m_detach(detach)
    {
    }
    ~ScopedGlobalEntry()
    {
        auto it = m_map.find(m_key);
        if (it == m_map.end()) return;
        if (m_detach) m_detach(it->second);
        delete it->second;
        m_map.erase(it);
    }
    ScopedGlobalEntry(const ScopedGlobalEntry&)            = delete;
    ScopedGlobalEntry& operator=(const ScopedGlobalEntry&) = delete;

private:
    Map& m_map;
    typename Map::key_type m_key;
    DetachFn m_detach;
};

void write_file(const std::string& path, const std::string& content)
{
    FILE* fp = fopen(path.c_str(), "wb");
    ASSERT_NE(fp, nullptr) << path;
    fwrite(content.data(), 1, content.size(), fp);
    fclose(fp);
}

}  // namespace

class LuaErrorPathTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CRoom room;
    CChar* me = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        saved_mgr  = g_item_mgr;
        g_item_mgr = &item_mgr;
        room.set("name", "lua_error_path_room");
        room.set("infinity", 1);

        me = new CChar();
        me->set_name("报错见证人", "lua_error_visitor");
        me->set("hp", (long)100);
        me->move(&room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        delete me;
        g_item_mgr = saved_mgr;
    }
};

// ====================================================================
// NPC：create / setup 与 22 个回调
// ====================================================================

TEST_F(LuaErrorPathTest, NpcCreateError_LogsAndKeepsNpcUsable)
{
    ErrorLogProbe probe("Lua npc create failed [t_broken_create_npc]");

    CNpc* npc = load_npc("t_broken_create_npc");
    probe.ExpectOnce();
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);

    // 报错不打断实例产出：create 里已写入的名字仍然可见
    EXPECT_STREQ(npc->name(), "坏创建伙计");
}

TEST_F(LuaErrorPathTest, NpcSetupError_LogsAndKeepsNpcUsable)
{
    ErrorLogProbe probe("Lua npc setup failed [t_error_npc]");

    CNpc* npc = load_npc("t_error_npc");
    probe.ExpectOnce();
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);

    EXPECT_STREQ(npc->name(), "报错伙计");
}

TEST_F(LuaErrorPathTest, NpcCallbackErrors_LogAndFallBackToBase)
{
    CNpc* npc = load_npc("t_error_npc");
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);
    CLuaNpc* script_npc = dynamic_cast<CLuaNpc*>(npc);
    ASSERT_NE(script_npc, nullptr);

    CChar* victim = new CChar();
    victim->set_name("挨打的", "lua_error_victim");
    victim->move(&room);

    CItem* gift = new CItem();
    gift->set_name("供品", "lua_error_gift");
    gift->move(&room);

    char ask[32]   = "天气如何";
    char comm[32]  = "bow";
    char arg[32]   = "";
    char tar[32]   = "";
    char skill[32] = "unarmed";
    char msg[32]   = "掌柜的，来一壶酒";
    int32_t damage = 7;

    {
        ErrorLogProbe probe("Lua do_talk failed [t_error_npc]");
        npc->do_talk(me, ask);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua handle_action failed [t_error_npc]");
        npc->handle_action(comm, me, arg);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua handle_action4 failed [t_error_npc]");
        npc->handle_action(comm, me, arg, tar);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua accept_object failed [t_error_npc]");
        npc->accept_object(me, gift);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc init failed [t_error_npc]");
        npc->init(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc die failed [t_error_npc]");
        npc->die();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc finish_kill failed [t_error_npc]");
        npc->finish_kill(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc is_defeated failed [t_error_npc]");
        npc->is_defeated(me, victim);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc do_look failed [t_error_npc]");
        npc->do_look(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc hit_ob failed [t_error_npc]");
        npc->hit_ob(me, victim, damage);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc chat_msg failed [t_error_npc]");
        npc->chat_msg();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc chat_msg_combat failed [t_error_npc]");
        npc->chat_msg_combat();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc greeting failed [t_error_npc]");
        script_npc->greeting(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc attempt_apprentice failed [t_error_npc]");
        npc->attempt_apprentice(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc prevent_learn failed [t_error_npc]");
        npc->prevent_learn(me, skill);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe(
            "Lua npc recognize_apprentice failed [t_error_npc]");
        npc->recognize_apprentice(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc accept_fight failed [t_error_npc]");
        npc->accept_fight(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc attack failed [t_error_npc]");
        npc->attack(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc relay_say failed [t_error_npc]");
        npc->relay_say(me, msg);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua npc relay_emote failed [t_error_npc]");
        npc->relay_emote(me, "nod");
        probe.ExpectOnce();
    }

    gift->move(nullptr);
    delete gift;
    victim->move(nullptr);
    delete victim;
}

TEST_F(LuaErrorPathTest, NpcInquiryError_LogsAfterTalkFallback)
{
    CNpc* npc = load_npc("t_error_npc");
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);

    CLuaNpc* script_npc = dynamic_cast<CLuaNpc*>(npc);
    ASSERT_NE(script_npc, nullptr);
    script_npc->set_inquiry_func("身世", "boom_inquiry", "身世");

    ErrorLogProbe probe("Lua inquiry boom_inquiry failed");

    // 脚本 do_talk 报错后回退询问逻辑，询问回调再报错也必须留痕
    char ask[32] = "身世";
    npc->do_talk(me, ask);
    probe.ExpectOnce();
}

TEST_F(LuaErrorPathTest, NpcReloadError_LogsAndKeepsInstance)
{
    CNpc* npc = load_npc("t_error_npc");
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);

    ErrorLogProbe probe("Lua npc reload failed [t_error_npc]");

    lua_reload_script("t_error_npc");
    probe.ExpectOnce();

    EXPECT_STREQ(npc->name(), "报错伙计");
}

// ====================================================================
// Item：create 与 12 个回调
// ====================================================================

TEST_F(LuaErrorPathTest, ItemCallbacks_LogAndFallBackToBase)
{
    std::unique_ptr<CContainer> holder = create_item("t_error_item", "");
    ASSERT_NE(holder, nullptr);

    ErrorLogProbe create_probe("Lua item create failed [t_error_item]");
    holder->set("base_name", "t_error_item");
    holder->create();
    holder->setup();
    create_probe.ExpectOnce();

    CLuaItem* item = dynamic_cast<CLuaItem*>(holder.get());
    ASSERT_NE(item, nullptr);

    CContainer* owned               = holder.release();
    g_item_list[owned->object_id()] = owned;
    ScopedGlobalEntry<MAP2ITEM> guard(g_item_list, owned->object_id(),
                                      detach_from_world);

    EXPECT_STREQ(owned->name(), "报错包裹");

    CChar* victim = new CChar();
    victim->set_name("挨打的", "lua_err_item_victim");
    victim->move(&room);

    char comm[32]  = "wield";
    char arg[32]   = "";
    char tar[32]   = "";
    int32_t damage = 3;

    {
        ErrorLogProbe probe("Lua item do_use failed [t_error_item]");
        item->do_use(me, owned);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item do_look failed [t_error_item]");
        item->do_look(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item Can_Use failed [t_error_item]");
        item->can_use();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item Can_Equip failed [t_error_item]");
        item->can_equip(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item hit_ob failed [t_error_item]");
        item->hit_ob(me, victim, damage);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item query_autoload failed [t_error_item]");
        item->query_autoload();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item handle_action failed [t_error_item]");
        item->handle_action(comm, me, arg);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item handle_action4 failed [t_error_item]");
        item->handle_action(comm, me, arg, tar);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item owner_is_killed failed [t_error_item]");
        item->owner_is_killed(victim);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item do_action failed [t_error_item]");
        item->do_action(0, me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item on_moved failed [t_error_item]");
        item->on_moved(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua item init failed [t_error_item]");
        item->init();
        probe.ExpectOnce();
    }

    victim->move(nullptr);
    delete victim;
}

// ====================================================================
// Room：CRoom 系（含 CContainer::create(flag) 协议分发）与 CFB 系
// ====================================================================

TEST_F(LuaErrorPathTest, RoomCallbacks_LogAndFallBackToBase)
{
    ErrorLogProbe create_probe("Lua room create failed [t_error_room]");
    ErrorLogProbe setup_probe("Lua room setup failed [t_error_room]");

    CRoom* room_ob = load_room("t_error_room");
    create_probe.ExpectOnce();
    setup_probe.ExpectOnce();
    ASSERT_NE(room_ob, nullptr);
    ScopedGlobalEntry<MAP2ROOM> guard(g_room_list, "t_error_room",
                                      detach_from_world);

    char comm[32] = "";
    char arg[32]  = "";
    char tar[32]  = "";

    {
        ErrorLogProbe probe("Lua room reset failed [t_error_room]");
        room_ob->reset();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua room init failed [t_error_room]");
        room_ob->init(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua room do_look failed [t_error_room]");
        room_ob->do_look(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua room handle_action failed [t_error_room]");
        room_ob->handle_action(comm, me, arg, tar);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua room valid_leave failed [t_error_room]");
        room_ob->valid_leave(me, "north");
        probe.ExpectOnce();
    }
    {
        // CRoom 自身没有 desc，回退分支由 CLuaRoomT 自己提供
        ErrorLogProbe probe("Lua room desc failed [t_error_room]");
        static_cast<CLuaRoom*>(room_ob)->desc(me);
        probe.ExpectOnce();
    }
    {
        // 走 CContainer::create(flag) 协议分发到脚本 create(self, nFlag)
        ErrorLogProbe probe("Lua fb create failed [t_error_room]");
        static_cast<CContainer*>(room_ob)->create(1);
        probe.ExpectOnce();
    }
}

TEST_F(LuaErrorPathTest, FbCallbacks_LogAndFallBackToBase)
{
    std::unique_ptr<IItemMgr> mgr = create_item_mgr();
    ASSERT_NE(mgr, nullptr);

    ErrorLogProbe probe("Lua fb create failed [t_error_fb]");
    CContainer* fb = mgr->load_fb("t_error_fb", 0);
    probe.ExpectOnce();
    ASSERT_NE(fb, nullptr);
    ScopedGlobalEntry<MAP2ITEM> guard(g_item_list, fb->object_id(),
                                      detach_from_world);

    CLuaFB* lua_fb = dynamic_cast<CLuaFB*>(fb);
    ASSERT_NE(lua_fb, nullptr);

    {
        ErrorLogProbe leave_probe("Lua fb valid_leave failed [t_error_fb]");
        lua_fb->valid_leave(me, "north");
        leave_probe.ExpectOnce();
    }
    {
        ErrorLogProbe desc_probe("Lua fb desc failed [t_error_fb]");
        lua_fb->desc(me);
        desc_probe.ExpectOnce();
    }
}

// ====================================================================
// Skill：create 与 13 个回调
// ====================================================================

TEST_F(LuaErrorPathTest, SkillCallbacks_LogAndFallBackToBase)
{
    ErrorLogProbe create_probe("Lua skill create failed [t_error_skill]");

    CSkill* skill = load_skill("t_error_skill");
    create_probe.ExpectOnce();
    ASSERT_NE(skill, nullptr);
    ScopedGlobalEntry<MAP2SKILL> guard(g_skill_list, "t_error_skill");

    EXPECT_STREQ(skill->name(), "报错武功");

    CChar* victim = new CChar();
    victim->set_name("挨打的", "lua_err_skill_victim");
    victim->move(&room);

    std::string usage = "unarmed";
    std::string combo = "unarmed+unarmed";
    std::string magic = "boom";
    int32_t damage    = 5;

    {
        // create 报错前法术表已登记，cast 因此能走到自己的报错分支
        ErrorLogProbe probe("Lua skill cast boom failed [t_error_skill]");
        skill->cast(magic, me, nullptr);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill query_action failed [t_error_skill]");
        skill->query_action(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill valid_learn failed [t_error_skill]");
        skill->valid_learn(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill valid_enable failed [t_error_skill]");
        skill->valid_enable(usage);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill practice_skill failed [t_error_skill]");
        skill->practice_skill(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill valid_force failed [t_error_skill]");
        skill->valid_force("1");
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill valid_force_pro failed [t_error_skill]");
        skill->valid_force_pro(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill valid_combine failed [t_error_skill]");
        skill->valid_combine(combo);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill query_dodge_msg failed [t_error_skill]");
        skill->query_dodge_msg(me);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill query_parry_msg failed [t_error_skill]");
        skill->query_parry_msg(nullptr);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill hit_ob failed [t_error_skill]");
        skill->hit_ob(me, victim, damage);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill type failed [t_error_skill]");
        skill->type();
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("Lua skill skill_improved failed [t_error_skill]");
        skill->skill_improved(me);
        probe.ExpectOnce();
    }

    victim->move(nullptr);
    delete victim;
}

// ====================================================================
// Quest：四个事件钩子
// ====================================================================

TEST_F(LuaErrorPathTest, QuestHooks_LogAndReturnSafeDefaults)
{
    CNpc* npc = load_npc("t_error_npc");
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> npc_guard(g_npc_list, npc->object_id(),
                                         detach_from_world);

    std::unique_ptr<CContainer> gift = create_item("t_error_item", "");
    ASSERT_NE(gift, nullptr);

    char ask[32] = "身世";
    {
        ErrorLogProbe probe("quest: has_quest failed");
        EXPECT_FALSE(quest_has_active(me));
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("quest: on_give_item failed");
        EXPECT_EQ(quest_accept_object(npc, me, gift.get()), 0);
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("quest: on_talk failed");
        EXPECT_FALSE(quest_do_talk(npc, me, ask));
        probe.ExpectOnce();
    }
    {
        ErrorLogProbe probe("quest: on_npc_killed failed");
        quest_npc_killed(me, npc);
        probe.ExpectOnce();
    }
}

// ====================================================================
// 引擎：重载三态（语法错 / 运行错 / 未返回 table）都保留旧版本
// ====================================================================

TEST_F(LuaErrorPathTest, ReloadFailures_LogAndKeepOldVersion)
{
    const char* name       = "t_reload_state_item";
    const std::string path = "tests/lua-tests/item/t_reload_state_item.lua";

    // 首份内容合法：首次加载成功并进 registry
    write_file(path, "return { create = function(self) end }\n");

    CLuaEngine* engine = CLuaEngine::instance();
    const int old_ref  = engine->load_script(name, LUA_SCRIPT_ITEM);
    ASSERT_NE(old_ref, LUA_NOREF);

    {
        // 语法错误：chunk 连编译都过不去
        ErrorLogProbe probe("Lua reload loadfile failed [t_reload_state_item]");
        write_file(path, "return { create = function(self) end\n");
        EXPECT_EQ(engine->load_script(name, LUA_SCRIPT_ITEM), old_ref);
        probe.ExpectOnce();
    }
    {
        // 运行错误：chunk 编译通过、执行即抛错
        ErrorLogProbe probe("Lua reload pcall failed [t_reload_state_item]");
        write_file(path, "error(\"runtime boom\")\nreturn {}\n");
        EXPECT_EQ(engine->load_script(name, LUA_SCRIPT_ITEM), old_ref);
        probe.ExpectOnce();
    }
    {
        // 返回类型错误：脚本没返回 table
        ErrorLogProbe probe(
            "Lua script t_reload_state_item must return a table");
        write_file(path, "return 42\n");
        EXPECT_EQ(engine->load_script(name, LUA_SCRIPT_ITEM), old_ref);
        probe.ExpectOnce();
    }

    // 三态失败后旧版本仍留在 registry 里且保持可用（表 + create 函数）
    lua_State* L = engine->get_state();
    ASSERT_NE(L, nullptr);
    lua_rawgeti(L, LUA_REGISTRYINDEX, old_ref);
    ASSERT_TRUE(lua_istable(L, -1)) << "旧版本脚本表必须仍在 registry";
    lua_getfield(L, -1, "create");
    EXPECT_TRUE(lua_isfunction(L, -1)) << "旧版本的 create 必须仍可取用";
    lua_pop(L, 2);

    std::remove(path.c_str());
}

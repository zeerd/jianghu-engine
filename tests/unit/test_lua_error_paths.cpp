// test_lua_error_paths.cpp — 脚本回调抛错时的报错与回退契约
//
// 契约：CLuaNpcT / CLuaItemT / CLuaRoomT / CLuaSkill 的每个脚本回调包装层、
// 以及通用挂点（pre_talk / pre_accept_object），在 lua_pcall 失败时必须
//   1. 打一条 ERROR 日志，指明是哪个脚本的哪个回调失败（调用方定位线上
//      故障的唯一线索：格式串 + 脚本名 + Lua 错误原文）；
//   2. 按该回调既定语义回退基类实现（或静默吞掉），绝不让 Lua 错误穿透
//      到 C++ 调用方。
// 脚本加载/重载的三态（语法错 / 运行错 / 未返回 table）同样归本文件所有：
// 旧版本必须保留可用，且三种失败各打一条不同的日志；重载结果的成败回传
// （reload_script 的 reloaded 出参、自动重载的计数与实例通知门槛）也归
// 本文件——失败不得被当成「已重载」上报。
//
// 夹具：tests/unit/lua-tests/{npc,item,room,skill}/ 下的 t_error_* 夹具，
// 每个回调都 error()，一条用例即可驱动整个报错面；通用挂点由用例直接往
// hooks 表注入抛错的钩子。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

#include "Char.h"
#include "Container.h"
#include "CReward.h"
#include "Global.h"
#include "Item.h"
#include "ItemMgr.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "RoomMgr.h"
#include "Skill.h"
#include "SkillMgr.h"
#include "Condition.h"  // ScopedGlobalEntry<MAP2CND> delete 需要完整类型
#include "User.h"
#include "LuaEngine.h"
#include "LuaItem.h"
#include "LuaNpc.h"
#include "LuaRoom.h"
#include "LuaSkill.h"
#include "mock/mock_itemmgr.h"
#include "test_log_hits.h"
#include "test_lua_env.h"

extern std::unique_ptr<CContainer> create_item(string base_name,
                                               string unique_id);
extern bool lua_reload_script(const char* name);

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

// 把文件 mtime 往回拨 seconds 秒：自动重载先做 mtime 快筛，同一秒内改写
// 会被当成「没变」跳过——测试不能 sleep 赌秒进位（禁用延时），直接改文件
// 时间戳制造确定性差异。各次写入用不同拨幅，彼此必然错开。
void rewind_file_mtime(const std::string& path, int seconds)
{
    std::error_code ec;
    auto tp = std::filesystem::last_write_time(path, ec);
    ASSERT_FALSE(ec) << path;
    std::filesystem::last_write_time(path, tp - std::chrono::seconds(seconds),
                                     ec);
    ASSERT_FALSE(ec) << path;
}

// 把文件 mtime 钉到指定时刻（构造「同一整秒、不同毫秒」这类确定性场景）
void set_file_mtime(const std::string& path, std::filesystem::file_time_type tp)
{
    std::error_code ec;
    std::filesystem::last_write_time(path, tp, ec);
    ASSERT_FALSE(ec) << path;
}

// 读文件全部字节（夹具改写前留档原文）
std::string read_file_bytes(const std::string& path)
{
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return "";
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string out(n > 0 ? static_cast<size_t>(n) : 0, '\0');
    if (n > 0) fread(&out[0], 1, static_cast<size_t>(n), fp);
    fclose(fp);
    return out;
}

// RAII：临时改写提交夹具，作用域结束（含 ASSERT 早退）还原原文并重建房间
// 索引——夹具是提交资产，绝不允许把改名/加注释的中间态留给后续用例。
class ScopedFileContent {
public:
    ScopedFileContent(const std::string& path, const std::string& modified)
        : m_path(path)
    {
        m_original = read_file_bytes(path);
        if (!m_original.empty()) write_file(path, modified);
    }
    ~ScopedFileContent()
    {
        if (m_original.empty()) return;
        write_file(m_path, m_original);
        CLuaEngine::instance()->load_room_index();
    }
    ScopedFileContent(const ScopedFileContent&)            = delete;
    ScopedFileContent& operator=(const ScopedFileContent&) = delete;

private:
    std::string m_path;
    std::string m_original;
};

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
// 通用挂点：钩子抛错时的报错与回退契约
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
    lua_State* L = g_LuaEngine->get_state();

    // 钩子抛错：CLuaHookCall 记一条带钩子名的 ERROR，行为按缺席处理
    luaL_dostring(L, "hooks.pre_talk = function() error('pre_talk boom') end");
    {
        ErrorLogProbe probe("[LuaHook] pre_talk failed");
        EXPECT_EQ(0, npc->do_talk(me, ask));
        probe.ExpectOnce();
    }
    luaL_dostring(L, "hooks.pre_talk = nil");

    luaL_dostring(
        L,
        "hooks.pre_accept_object = function() error('pre_accept_object boom') "
        "end");
    {
        ErrorLogProbe probe("[LuaHook] pre_accept_object failed");
        EXPECT_EQ(0, npc->accept_object(me, gift.get()));
        probe.ExpectOnce();
    }
    luaL_dostring(L, "hooks.pre_accept_object = nil");
}

// ====================================================================
// 引擎：重载三态（语法错 / 运行错 / 未返回 table）都保留旧版本
// ====================================================================

TEST_F(LuaErrorPathTest, ReloadFailures_LogAndKeepOldVersion)
{
    const char* name = "t_reload_state_item";
    const std::string path
        = "tests/unit/lua-tests/item/t_reload_state_item.lua";

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

    // 结果语义：失败回传「仍持有可用旧引用」，但 reloaded 出参必须是 false
    // ——两个问题不能混：调用方拿引用继续跑，结果上报（日志/回执/广播）
    // 看出参。旧实现混为一谈，失败被当成成功上报。
    {
        bool ok = true;
        EXPECT_EQ(engine->reload_script("item/t_reload_state_item", &ok),
                  old_ref)
            << "失败仍返回可用旧引用";
        EXPECT_FALSE(ok) << "失败必须回传 reloaded=false";
    }

    // 修好后回传 true，且新内容真的生效（marker 字段可读）
    write_file(path, "return { create = function(self) end, marker = 1 }\n");
    {
        bool ok = false;
        engine->reload_script("item/t_reload_state_item", &ok);
        EXPECT_TRUE(ok) << "修复后必须回传成功";
    }
    EXPECT_TRUE(engine->get_script_bool_field("t_reload_state_item",
                                              LUA_SCRIPT_ITEM, "marker"))
        << "成功重载后新内容必须生效";

    std::remove(path.c_str());
}

// ====================================================================
// 自动重载（check_for_reload）：计数与实例通知都以成功为门槛
// ====================================================================

// 失败的自动重载既不计入「已重载」，也不得通知实例重建——旧实现在拿
// LUA_NOREF 判真值，失败保留的旧 ref(≥0) 恒真：失败照样记成功日志、
// 照样 notify（实例每 5 秒拿旧版本 purge+create 一次，create 的副作用
// 重复执行）。以实例 create 计数为证：通知一旦发生计数必增。
TEST_F(LuaErrorPathTest, CheckForReload_GatesCountAndNotifyOnSuccess)
{
    const char* name       = "t_autoreload_npc";
    const std::string path = "tests/unit/lua-tests/npc/t_autoreload_npc.lua";
    const std::string good_script
        = "local npc = {}\n"
          "function npc.create(self)\n"
          "    self:set_name(\"重载计数员\", \"t_autoreload_npc\")\n"
          "    self:set(\"create_runs\","
          " (self:query(\"create_runs\") or 0) + 1)\n"
          "end\n"
          "return npc\n";

    write_file(path, good_script);
    CLuaEngine* engine = CLuaEngine::instance();
    CNpc* npc          = load_npc(name);
    ASSERT_NE(npc, nullptr);
    ScopedGlobalEntry<MAP2NPC> guard(g_npc_list, npc->object_id(),
                                     detach_from_world);
    ASSERT_EQ((long)npc->query("create_runs"), 1) << "首建跑过一次 create";

    // 先空转一遍：把世界里其他 mtime 漂移的条目同步掉，本用例的计数
    // 不受别的夹具串扰
    engine->check_for_reload();

    // 写坏（语法错误）+ 拨动 mtime：尝试自动重载但失败——不计数、不通知
    {
        ErrorLogProbe probe("Lua reload loadfile failed [t_autoreload_npc]");
        write_file(path, "local npc = {}\nfunction npc.create(self)\n");
        rewind_file_mtime(path, 200);
        EXPECT_EQ(engine->check_for_reload(), 0) << "失败不得计入重载数";
        probe.ExpectOnce();
    }
    EXPECT_EQ((long)npc->query("create_runs"), 1) << "失败重载不得通知实例重建";

    // 修好 + 拨动 mtime：内容必须与首载不同——同内容哈希不变会被判成
    // 「mtime 抖动、内容未变」的 noop 分支（那正是探针场景的语义），故
    // 末尾追加一行注释再写回，语义不变、字节必变
    write_file(path, good_script + "-- repaired, content differs\n");
    rewind_file_mtime(path, 100);
    EXPECT_EQ(engine->check_for_reload(), 1);
    EXPECT_EQ((long)npc->query("create_runs"), 2) << "成功重载必须通知实例重建";

    // 内容未再变化：mtime 已同步，空转不计数
    EXPECT_EQ(engine->check_for_reload(), 0);

    std::remove(path.c_str());
}

// ====================================================================
// 结果语义的三处补齐（2026-11-05 批次）：索引重建 / 状态缓存 / 快筛精度
// ====================================================================

// 单个 reload 必须重建房间中文名索引：改掉房间 name 后，只调 reload（不手动
// load_room_index）新名就要能解析——否则手动 reload 后 load_room 仍按旧名
// 解析（陈旧到下一次全量/自动重载），与 reload all / 自动重载的行为不一致。
TEST_F(LuaErrorPathTest, ReloadRoomScriptRefreshesNameIndex)
{
    const std::string path = "tests/unit/lua-tests/room/fixture_blank_room.lua";
    std::string original   = read_file_bytes(path);
    ASSERT_FALSE(original.empty()) << path;

    // 只替换表级 name 的值（第一处 name = "..."），其余字节不动。
    // 探针名用 ASCII：引擎内部房间名统一转 GBK（load_file_as_gbk），中文
    // 字面量两侧编码不同会查表不中，ASCII 则天然一致
    size_t k = original.find("name = \"");
    ASSERT_NE(k, std::string::npos) << "夹具缺 name 字段";
    size_t e = original.find('"', k + 8);
    ASSERT_NE(e, std::string::npos);
    std::string modified
        = original.substr(0, k + 8) + "e2e_renamed_probe" + original.substr(e);
    ScopedFileContent restore(path, modified);

    EXPECT_TRUE(lua_reload_script("fixture_blank_room")) << "重载必须成功";

    // 不显式调 load_room_index：索引重建必须由 reload 自己完成
    EXPECT_STREQ(CLuaEngine::instance()->resolve_room_name("e2e_renamed_probe"),
                 "fixture_blank_room")
        << "单个 reload 未重建房间名索引：改名后新名解析不到";
    // 还原由 ScopedFileContent 析构完成（含索引重建）
}

// 自动重载与手动 reload 必须同语义：脚本换新后技能/condition 缓存一并摘除。
// 旧实现只有手动路径摘——自动重载后 CSkill/CCondition 仍持旧脚本引用，
// 改 skill 脚本行为照旧（改了不生效）。
TEST_F(LuaErrorPathTest, CheckForReload_ReleasesSkillAndConditionCaches)
{
    const std::string skill_path
        = "tests/unit/lua-tests/skill/enable_sample.lua";
    const std::string cnd_path = "tests/unit/lua-tests/condition/poison.lua";
    std::string sk_orig        = read_file_bytes(skill_path);
    std::string cd_orig        = read_file_bytes(cnd_path);
    ASSERT_FALSE(sk_orig.empty()) << skill_path;
    ASSERT_FALSE(cd_orig.empty()) << cnd_path;

    // 挂载：load_skill 建技能缓存并把脚本载入 m_scripts；condition 的脚本
    // 是惰性载入的（init 才 load_script），走 apply_condition 触发
    ASSERT_NE(load_skill("enable_sample"), nullptr);
    auto tmp = std::unique_ptr<CChar>(new CChar());
    tmp->set("name", "缓存摘除探针");
    tmp->set("id", "cnd_probe");
    bool applied = tmp->apply_condition("poison", 3, 0);
    tmp->set_heart_beat(0);  // 无论成败先摘心跳，析构安全
    ASSERT_TRUE(applied);
    ASSERT_EQ(g_skill_list.count("enable_sample"), 1u);
    ASSERT_EQ(g_condition_list.count("poison"), 1u);

    // 内容必须变字节（同内容会被 noop 分支吞掉）：追加注释行，语义不变
    ScopedFileContent sk_guard(skill_path, sk_orig + "-- e2e reload probe\n");
    ScopedFileContent cd_guard(cnd_path, cd_orig + "-- e2e reload probe\n");
    ScopedGlobalEntry<MAP2SKILL> skill_guard(g_skill_list, "enable_sample");
    ScopedGlobalEntry<MAP2CND> cnd_guard(g_condition_list, "poison");

    EXPECT_GE(CLuaEngine::instance()->check_for_reload(), 1);

    EXPECT_EQ(g_skill_list.count("enable_sample"), 0u)
        << "自动重载未摘技能缓存：改 skill 脚本后仍拿到旧对象";
    EXPECT_EQ(g_condition_list.count("poison"), 0u)
        << "自动重载未摘 condition 缓存";
}

// 快筛必须看得见「同一秒内的第二次编辑」：记录与新写入落在同一整秒时，
// 秒级比较会判相等直接跳过——最后一次保存恰好落在重载同秒，改动就永不
// 生效。FILETIME(100ns) 粒度下同秒不同毫秒必须放行并按哈希确认重载。
TEST_F(LuaErrorPathTest, CheckForReload_SameSecondEditStillReloads)
{
    const char* name       = "t_samesec_item";
    const std::string path = "tests/unit/lua-tests/item/t_samesec_item.lua";
    write_file(path, "return { create = function(self) end, first = 1 }\n");

    // mtime 钉到「整秒 +200ms」，load 记录它
    auto now     = std::filesystem::last_write_time(path);
    auto floored = now - (now.time_since_epoch() % std::chrono::seconds(1));
    auto base    = floored + std::chrono::milliseconds(200);
    set_file_mtime(path, base);

    CLuaEngine* engine = CLuaEngine::instance();
    ASSERT_NE(engine->load_script(name, LUA_SCRIPT_ITEM), LUA_NOREF);

    // 第二次编辑：内容变、mtime 钉回同一整秒的 +700ms——秒级比较会判相等
    // 跳过（旧实现的漏检点），本用例按「新内容必须生效」判定
    write_file(path, "return { create = function(self) end, second = 1 }\n");
    set_file_mtime(path, base + std::chrono::milliseconds(500));

    engine->check_for_reload();
    EXPECT_TRUE(engine->get_script_bool_field(name, LUA_SCRIPT_ITEM, "second"))
        << "同一秒内的第二次编辑被 mtime 快筛漏掉：新内容未生效";

    std::remove(path.c_str());
}

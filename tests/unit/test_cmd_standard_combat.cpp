// test_cmd_standard_combat.cpp - Unit tests for CmdStandardCombat.cpp
//
// Tests combat game state.
//
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "Npc.h"
#include "CmdStandard.h"
#include "Equip.h"
#include "FightRoom.h"
#include "Global.h"
#include "HeartBeat.h"
#include "test_lua_env.h"
#include "test_scoped_room.h"
#include "test_state_recorder.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"

extern char g_notify_fail_msg[512];
extern int g_current_time;

namespace {
// 可堆叠物件：数量>1 搬入不触发"涉嫌复制"闸门（CAN_COMBINE 豁免）
class StackableItem : public CItem {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == CAN_COMBINE) return 1;
        return CItem::query_attr(attr);
    }
};

CUser* MakeGateUser(const char* name, const char* id, long level)
{
    CUser* u = new CUser();
    u->set_name(name, id);
    u->set("id", id);
    u->set("level", level);
    u->set("hp", (long)100);
    u->set("eff_hp", (long)100);
    return u;
}

// cmd_hit_ob/do_bishi 经 g_item_mgr 载入战斗房；登记后由夹具统一回收
std::vector<CContainer*> g_created_fight_rooms;

CContainer* combat_item_factory(const char* base_name)
{
    if (strcmp(base_name, "fight_room") == 0) {
        CFightRoom* fr = new CFightRoom();
        fr->set("base_name", base_name);
        g_created_fight_rooms.push_back(fr);
        return fr;
    }
    return nullptr;
}
}  // namespace

class CmdStandardCombatTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallKill(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"kill", me, target, arg);
    }
    static int CallCast(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"cast", me, target, arg);
    }
    static int CallFight(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"fight", me, target, arg);
    }
    static int CallFollow(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"follow", me, target, arg);
    }
    static int CallHalt(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"halt", me, target, arg);
    }
    static int CallJoin(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"join", me, target, arg);
    }
    static int CallGuard(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"guard", me, target, arg);
    }

    // 给角色装备指定类型的兵器（cast 的武器校验需要）
    CEquip* WieldSword(CChar* who, const char* skill_type)
    {
        auto* eq = new CEquip();
        eq->set_name("长剑", "sword_eq");
        eq->set("base_name", "sword_eq");
        eq->set("skill_type", skill_type);
        CMapping apply;
        apply.set("damage", 1L);
        apply.set("desc", "攻击");
        eq->set("apply", apply);
        eq->set("wield_position", (int32_t)WIELD_HAND);
        eq->move(who);
        eq->do_action(CChar::DO_WIELD, who);
        m_equips.push_back(eq);
        return eq;
    }

    // 造一枚暗器放入身上（skill_type=throwing：偷袭的暗器来源）
    CItem* GiveThrowing(CChar* who, int32_t amount, const char* id = "dart")
    {
        CItem* it = (amount > 1) ? new StackableItem() : new CItem();
        it->set_name("飞镖", id);
        it->set("base_name", "dart");
        it->set("skill_type", "throwing");
        it->set_amount(amount);
        it->move(who);
        m_items.push_back(it);
        return it;
    }

    // 观战场景：一间空的战斗房放在 me 所在房间内，返回该房供用例设标志
    CFightRoom* MakeGuardScene(const char* name, const char* id)
    {
        CFightRoom* fr = new CFightRoom();
        fr->set_name(name, id);
        fr->move(room);
        guard_rooms.push_back(fr);
        return fr;
    }

    std::vector<CFightRoom*> guard_rooms;
    std::vector<CEquip*> m_equips;
    std::vector<CItem*> m_items;  // 随身暗器等：脱身后统一销毁
    // SetUp 时快照：TearDown 只回收本用例新增的自毁对象（end_fight 的战房）
    std::vector<CContainer*> m_live_at_setup;
    // kill/fight 的开战路径与 do_bishi 都经 g_item_mgr 载入战斗房，
    // expell/存档类链路会落库——夹具注入并按契约归还
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;
    MockDatabase* db    = nullptr;

    CChar* me;
    CChar* target;
    CContainer* room;
    CUser* join_player  = nullptr;
    CFightRoom* join_fr = nullptr;
    CChar* join_npc     = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        g_notify_fail_msg[0] = '\0';
        db                   = new MockDatabase();
        g_db                 = db;
        saved_mgr            = g_item_mgr;
        mgr                  = new CMockItemMgr();
        mgr->load_factory    = &combat_item_factory;
        g_item_mgr           = mgr;

        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->set("combat_exp", (long)100000);
        me->move(room);

        target = new CChar();
        target->set("name", "目标");
        target->set("id", "target1");
        target->set("level", (long)50);
        target->set("hp", (long)100);
        target->set("max_hp", (long)100);
        target->set("combat_exp", (long)100000);
        target->move(room);
    }

    void TearDown() override
    {
        // 放行用例会把 me 移进战斗房：先把人移回房间再销毁战斗房。
        // 角色若还留在战房里，fr 先删会让其 environment 悬垂（UAF），
        // 故 target/me 一并在 guard_rooms 销毁前摘出
        me->move(room);
        target->move(nullptr);
        for (CFightRoom* fr : guard_rooms) {
            fr->set_heart_beat(0);  // begin_fight 注册过心跳
            fr->move(nullptr);
            delete fr;
        }
        guard_rooms.clear();

        for (CEquip* eq : m_equips) {
            eq->set_heart_beat(0);
            eq->move(nullptr);
            delete eq;
        }
        m_equips.clear();

        for (CUser* caster : casters) {
            caster->move(nullptr);
            delete caster;
        }
        casters.clear();

        if (join_npc) join_npc->move(nullptr);
        if (join_player) join_player->move(nullptr);
        if (join_fr) join_fr->move(nullptr);
        delete join_npc;
        delete join_player;
        delete join_fr;
        target->move(nullptr);
        me->move(nullptr);

        // 战斗房的 environment 指向 room：必须在 room 销毁前摘除，
        // 否则 fr->move(nullptr) 踩已释放房间（UAF），且 vector 残留会级联毒化
        // 后续所有用例的 TearDown
        // 延迟析构列表：只回收本用例新增的对象（end_fight 自毁的战房）
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete ob;
            }
        }

        // 工厂造的战斗房（cmd_hit_ob/do_bishi）：先摘心跳再销毁
        for (CContainer* fr : g_created_fight_rooms) {
            fr->set_heart_beat(0);
            fr->move(nullptr);
            delete fr;
        }
        g_created_fight_rooms.clear();

        room->move(nullptr);
        // 随身暗器：先脱身（脱离角色）再销毁，角色析构不会重复回收
        for (auto* it : m_items) {
            it->set_heart_beat(0);
            it->move(nullptr);
            delete it;
        }
        m_items.clear();
        delete target;
        delete me;
        delete room;

        // 共享管理器按契约归还（test_env 的 g_testItemMgr），不得置空
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr  = nullptr;
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    // join_battle 场景：参战玩家与战斗房在房间内，一名 B 队 NPC 在战斗房中
    void MakeJoinScene(const char* npc_name, const char* npc_id)
    {
        join_player = MakeGateUser("参战者", "join_user", 50);
        join_player->move(room);
        // 玩家进房会挂 client/loading 等客户端回执，未回执即对 present(id)
        // 不可见
        join_player->delete_temp("client/loading");

        join_fr = new CFightRoom();
        join_fr->set_name("战斗", "join_fr");
        join_fr->move(room);

        join_npc = new CChar();
        join_npc->set_name(npc_name, npc_id);
        join_npc->set_temp("fight/fight_team", CFightRoom::FIGHT_TEAM_B);
        join_npc->move(join_fr);
    }

    // ---------------- cast 的 PK 敌意标记矩阵 ----------------
    // 施法者学会 pk_gate_sample（special 技能，免技能映射与武器校验），
    // 法术名经 `cast pk_gate_sample <magic>` 传入；target 为 0 表示无目标施放。
    CUser* MakeCaster(const char* id)
    {
        ensure_lua_env();
        g_current_time = 1000000;  // pending/PK 以时钟值为标记，非零才可观测

        CUser* caster = MakeGateUser("施法者", id, 50);
        caster->set_skill("pk_gate_sample", 100);
        caster->move(room);
        caster->delete_temp("client/loading");
        casters.push_back(caster);
        return caster;
    }

    int CastPkGate(CUser* caster, int32_t target, const char* magic)
    {
        char arg[64];
        snprintf(arg, sizeof(arg), "pk_gate_sample %s", magic);
        return CallCast(caster, target, arg);
    }

    std::vector<CUser*> casters;
};

TEST_F(CmdStandardCombatTest, Kill_NoTarget)
{
    int result = CallKill(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardCombatTest, Kill_SelfTargetRejected)
{
    // 目标有效（自己在房）→ cmd_hit_ob 的“不能杀自己”守卫拒绝，不开战
    int result = CallKill(me, me->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(me->is_fighting());
}

TEST_F(CmdStandardCombatTest, Cast_NoArgs)
{
    int result = CallCast(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardCombatTest, Fight_NoTarget)
{
    int result = CallFight(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardCombatTest, Halt_NotFighting)
{
    int result = CallHalt(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

// 玩家互斗的出手准入：用例内未改写 attack_gate，默认数据集（lua-default）的
// 75 级新手保护在 CChar::cmd_hit_ob 拒绝本次 kill——kill 命令层不自行询问准入，
// 一切动手路径在 cmd_hit_ob 过同一道闸。
TEST_F(CmdStandardCombatTest, Kill_DefaultNewbieGate_Rejected)
{
    ensure_lua_env();

    CUser* killer = MakeGateUser("杀手", "kill_gate_k2", 50);
    CUser* victim = MakeGateUser("帮众", "kill_gate_v2", 50);
    killer->move(room);
    victim->move(room);
    // 玩家进房会挂 client/loading 等客户端回执，未回执即对 present(id) 不可见
    killer->delete_temp("client/loading");
    victim->delete_temp("client/loading");

    g_notify_fail_msg[0] = '\0';
    int result           = CallKill(killer, victim->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "才入江湖，何必动刀动枪。")
        << "默认数据集按等级拒绝未达标的新手互斗";

    victim->move(nullptr);
    killer->move(nullptr);
    delete victim;
    delete killer;
}

// ---------------- join 战斗：NPC 方代表筛选（hooks.combat_rule 数据集规则）
// ----------------
TEST_F(CmdStandardCombatTest, Join_NpcSideRejectsOrdinaryNpcTeam)
{
    ensure_lua_env();
    MakeJoinScene("山贼", "join_thug");

    g_notify_fail_msg[0] = '\0';
    int result = CallJoin(join_player, join_fr->object_id(), (char*)"1");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "此战斗禁止加入NPC一方!");
}

// 钩子缺席时回退为数据集无关的候选筛选：特定人物不再豁免
TEST_F(CmdStandardCombatTest, Join_FallbackRejectsContentExemptNpc)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    ASSERT_EQ(
        luaL_dostring(
            L, "__orig_join_cr = hooks.combat_rule; hooks.combat_rule = nil"),
        LUA_OK);

    MakeJoinScene("天山铁姥", "join_tielao_fb");

    g_notify_fail_msg[0] = '\0';
    int result = CallJoin(join_player, join_fr->object_id(), (char*)"1");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "此战斗禁止加入NPC一方!");

    EXPECT_EQ(
        luaL_dostring(
            L, "hooks.combat_rule = __orig_join_cr; __orig_join_cr = nil"),
        LUA_OK);
}

// ---------------- cast 的 PK 敌意标记矩阵（hostile 标志 × target 形态）
// ---------------- 敌意 + 指定他人 + 玩家受害者 -> 挂 pending/PK
TEST_F(CmdStandardCombatTest, Cast_HostileOnPlayerSetsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_h_player");
    CUser* victim = MakeGateUser("受害者", "cast_pk_v1", 50);
    victim->move(room);
    victim->delete_temp("client/loading");

    EXPECT_EQ(CastPkGate(caster, victim->object_id(), "strike"), 1);
    EXPECT_NE(caster->query_temp("pending/PK"), 0);

    victim->move(nullptr);
    delete victim;
}

// 非敌意 + 指定他人 + 玩家受害者 -> 不挂（对玩家疗伤/扶持不算动手）
TEST_F(CmdStandardCombatTest, Cast_NonHostileOnPlayerKeepsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_nh_player");
    CUser* victim = MakeGateUser("受助者", "cast_pk_v2", 50);
    victim->move(room);
    victim->delete_temp("client/loading");

    EXPECT_EQ(CastPkGate(caster, victim->object_id(), "mend"), 1);
    EXPECT_EQ(caster->query_temp("pending/PK"), 0);

    victim->move(nullptr);
    delete victim;
}

// 敌意 + 无目标（波及全场，狮子吼一类）+ 无玩家受害者 -> 仍挂
TEST_F(CmdStandardCombatTest, Cast_HostileAreaSetsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_h_area");

    EXPECT_EQ(CastPkGate(caster, 0, "shout"), 1);
    EXPECT_NE(caster->query_temp("pending/PK"), 0);
}

// 非敌意 + 无目标（自身姿态/调理）-> 不挂（锁死"不以形态推敌意"）
TEST_F(CmdStandardCombatTest, Cast_NonHostileAreaKeepsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_nh_area");

    EXPECT_EQ(CastPkGate(caster, 0, "focus"), 1);
    EXPECT_EQ(caster->query_temp("pending/PK"), 0);
}

// 敌意 + 指定他人但受害者是 NPC -> 不挂（无玩家受害者）
TEST_F(CmdStandardCombatTest, Cast_HostileOnNpcKeepsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_h_npc");

    EXPECT_EQ(CastPkGate(caster, target->object_id(), "strike"), 1);
    EXPECT_EQ(caster->query_temp("pending/PK"), 0)
        << "target 是普通 CChar（非玩家），不构成 PK";
}

// 敌意 + 指定他人但受害者是自己 -> 不挂
TEST_F(CmdStandardCombatTest, Cast_HostileOnSelfKeepsPendingPk)
{
    CUser* caster = MakeCaster("cast_pk_h_self");

    EXPECT_EQ(CastPkGate(caster, caster->object_id(), "strike"), 1);
    EXPECT_EQ(caster->query_temp("pending/PK"), 0);
}

// ---------------- 观战准入（guard）：只读同轴标志，无数据集名称知识
// ----------------
TEST_F(CmdStandardCombatTest, Guard_PlainFightRoomAllowsSpectating)
{
    CFightRoom* fr = MakeGuardScene("切磋", "guard_plain");

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_EQ(me->environment(), fr) << "放行后进入战斗房观战";
}

TEST_F(CmdStandardCombatTest, Guard_NoGuardTextRejects)
{
    CFightRoom* fr = MakeGuardScene("月屏战斗", "guard_txt");
    fr->set("no_guard", "一道光亮眩花了你的眼睛。");

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "一道光亮眩花了你的眼睛。");
    EXPECT_NE(me->environment(), fr);
}

// no_guard 存成数值：querystr 对 T_NUMBER 返回空串，落到默认文案
TEST_F(CmdStandardCombatTest, Guard_NoGuardFlagUsesDefaultText)
{
    CFightRoom* fr = MakeGuardScene("封锁战斗", "guard_flag");
    fr->set("no_guard", (long)1);

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "此战斗不允许观战!");
}

TEST_F(CmdStandardCombatTest, Guard_NoGuardBitRejects)
{
    CFightRoom* fr = MakeGuardScene("寻宝战斗", "guard_bit");
    fr->set("room_locked", FIGHT_NO_GUARD);

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "此战斗被锁住，无法观战。");
}

// FIGHT_LOCKED 含观战位，锁死战斗同样不可观战
TEST_F(CmdStandardCombatTest, Guard_FightLockedBitRejects)
{
    CFightRoom* fr = MakeGuardScene("镖车战斗", "guard_locked");
    fr->set("room_locked", FIGHT_LOCKED);

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_STREQ(g_notify_fail_msg, "此战斗被锁住，无法观战。");
}

// 只禁脱战（FIGHT_NO_FLEE）的战斗不挡观战——比武招亲决赛房即此形态
TEST_F(CmdStandardCombatTest, Guard_NoFleeOnlyAllowsSpectating)
{
    CFightRoom* fr = MakeGuardScene("比武招亲", "guard_flee");
    fr->set("room_locked", FIGHT_NO_FLEE);

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_EQ(me->environment(), fr);
}

// 巫师绕过 no_guard 与锁位两道门
TEST_F(CmdStandardCombatTest, Guard_WizardBypassesBothGates)
{
    CFightRoom* fr = MakeGuardScene("封锁战斗", "guard_wiz");
    fr->set("no_guard", "此战斗不允许观战!");
    fr->set("room_locked", FIGHT_LOCKED);
    me->set("grant", (long)CMainObject::GRANT_WIZARD);

    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, fr->object_id(), (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_EQ(me->environment(), fr);
}

// 目标不是战斗房 -> 直接失败，不改动位置
TEST_F(CmdStandardCombatTest, Guard_NonFightRoomRejected)
{
    g_notify_fail_msg[0] = '\0';
    int result           = CallGuard(me, target->object_id(), (char*)"");
    EXPECT_EQ(result, 0);
    EXPECT_EQ(me->environment(), room);
}

// =====================================================================
// follow：房间禁令/无目标/自取消/指定跟随
// =====================================================================
TEST_F(CmdStandardCombatTest, Follow_NoFollowRoom_Rejected)
{
    room->set("no_follow", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFollow(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止跟随他人"));
}

TEST_F(CmdStandardCombatTest, Follow_NoTarget_Rejected)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFollow(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你要跟随谁"));
}

TEST_F(CmdStandardCombatTest, Follow_Self_CancelsLeader)
{
    me->set_leader(target);  // 预置跟随，再对自己下达 follow → 取消
    EXPECT_EQ(1, CallFollow(me, me->object_id(), (char*)""));
    EXPECT_EQ(nullptr, me->query_leader()) << "对自己 follow 即取消跟随";
}

TEST_F(CmdStandardCombatTest, Follow_Other_SetsLeader)
{
    EXPECT_EQ(1, CallFollow(me, target->object_id(), (char*)""));
    EXPECT_EQ(target, me->query_leader()) << "记录跟随对象";
}

// =====================================================================
// halt：战斗房观战退场 / 锁逃 / 阈值收场 / 忙碌 / AP 不足 / 房外中断
// =====================================================================
TEST_F(CmdStandardCombatTest, Halt_SpectatorLeavesFightRoom)
{
    CFightRoom* fr = MakeGuardScene("演武场", "halt_sp");
    me->move(fr);
    EXPECT_EQ(1, CallHalt(me, 0, (char*)""));
    EXPECT_EQ(room, me->environment()) << "观战者 halt 离开战斗房";
}

TEST_F(CmdStandardCombatTest, Halt_NoFleeLock_Rejected)
{
    CFightRoom* fr = MakeGuardScene("锁笼决斗", "halt_lock");
    fr->set("room_locked", FIGHT_NO_FLEE);
    me->move(fr);
    me->set_temp("fight/is_fighting", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallHalt(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("该战斗被锁住"));
}

TEST_F(CmdStandardCombatTest, Halt_ThresholdMatch_EndsFight)
{
    CFightRoom* fr = MakeGuardScene("点到为止", "halt_th");
    fr->begin_fight(1, 0);  // 阈值模式：halt 直接收场不消耗 AP
    me->move(fr);
    me->set_temp("fight/is_fighting", 1);

    EXPECT_EQ(1, CallHalt(me, 0, (char*)""));
    EXPECT_EQ(room, me->environment()) << "阈值收场把成员送出战圈";
    // end_fight 会自毁战斗房：摘出回收清单，由延迟析构列表统一清理
    guard_rooms.pop_back();
}

TEST_F(CmdStandardCombatTest, Halt_BusyInFight_Rejected)
{
    CFightRoom* fr = MakeGuardScene("鏖战", "halt_busy");
    me->move(fr);
    me->set_temp("fight/is_fighting", 1);
    me->start_busy(1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallHalt(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你没空逃跑"));
}

TEST_F(CmdStandardCombatTest, Halt_LowAp_Rejected)
{
    CFightRoom* fr = MakeGuardScene("拉锯战", "halt_ap");
    me->move(fr);
    me->set_temp("fight/is_fighting", 1);
    me->set_temp("fight/ap", (long)10);  // 逃走要 50 AP

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallHalt(me, 0, (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("AP"));
}

TEST_F(CmdStandardCombatTest, Halt_BusyOutside_Interrupts)
{
    me->start_busy(1);
    EXPECT_EQ(1, CallHalt(me, 0, (char*)"")) << "房外忙碌时 halt 为中断指令";
}

// =====================================================================
// fight：禁令/跟随/已在战圈/生死忙闲/玩家约战/开口NPC比试/沉默目标开打
// =====================================================================
TEST_F(CmdStandardCombatTest, Fight_NoFightRoom_Rejected)
{
    room->set("no_fight", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(me, target->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止战斗"));
}

TEST_F(CmdStandardCombatTest, Fight_PartyFollow_Rejected)
{
    CUser* boss = MakeGateUser("镖头", "f_boss1", 80);
    boss->move(room);
    boss->delete_temp("client/loading");
    g_user_list[boss->object_id()] = boss;

    CUser* u = MakeGateUser("随行者", "f_fol1", 80);
    u->move(room);
    u->delete_temp("client/loading");
    u->set_temp("party/master", "f_boss1");
    u->set_temp("party/follow", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(u, target->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你正处于组队跟随模式"));

    g_user_list.erase(boss->object_id());
    boss->move(nullptr);
    delete boss;
    u->move(nullptr);
    delete u;
}

TEST_F(CmdStandardCombatTest, Fight_AlreadyInBattleRoom_Returns1)
{
    CFightRoom* fr = MakeGuardScene("群殴", "fight_in");
    CChar* spar    = new CChar();
    spar->set_name("陪练", "spar1");
    spar->move(fr);
    me->move(fr);

    EXPECT_EQ(1, CallFight(me, spar->object_id(), (char*)""))
        << "已在战圈直接放行";
    spar->move(nullptr);
    delete spar;
}

TEST_F(CmdStandardCombatTest, Fight_TargetNotLiving_Rejected)
{
    target->set_temp("disabled", "1");
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(me, target->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("已经无法战斗"));
}

TEST_F(CmdStandardCombatTest, Fight_Self_Rejected)
{
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(me, me->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你不能攻击自己"));
}

TEST_F(CmdStandardCombatTest, Fight_TargetBusy_Rejected)
{
    target->start_busy(1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(me, target->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("对方没空理你"));
}

TEST_F(CmdStandardCombatTest, Fight_UserProposal_SetsPending)
{
    CUser* victim = MakeGateUser("好手", "f_prop1", 50);
    victim->move(room);
    victim->delete_temp("client/loading");

    EXPECT_EQ(1, CallFight(me, victim->object_id(), (char*)""));
    EXPECT_EQ(victim->object_id(), me->query_temp("pending/fight"))
        << "对玩家先约战，挂起等应战";

    victim->move(nullptr);
    delete victim;
}

TEST_F(CmdStandardCombatTest, Fight_SpeakingNpc_BishiStarts)
{
    target->set("can_speak", 1);
    me->set_temp("pending/fight", (long)999);

    EXPECT_EQ(1, CallFight(me, target->object_id(), (char*)""));
    EXPECT_EQ(0, me->query_temp("pending/fight")) << "比试开始即清掉挂起意向";
}

TEST_F(CmdStandardCombatTest, Fight_SilentTarget_AttacksDirectly)
{
    auto* attacker = new StateRecordingUser();
    attacker->set_name("醉汉", "f_silent1");
    attacker->set("hp", (long)100);
    attacker->set("eff_hp", (long)100);
    attacker->move(room);
    attacker->delete_temp("client/loading");

    EXPECT_EQ(1, CallFight(attacker, target->object_id(), (char*)""));
    EXPECT_TRUE(attacker->has("大喝一声")) << "不开口的目标直接开打";

    attacker->move(nullptr);
    delete attacker;
}

TEST_F(CmdStandardCombatTest, Fight_SpeakingNpc_CanDecline)
{
    // HP 满 + friendly：accept_fight 直接婉拒，不出比试房
    CNpc* npc = new CNpc();
    npc->set_name("武痴", "bishi_npc");
    npc->set("can_speak", 1);
    npc->set("max_hp", (long)100);
    npc->set("hp", (long)100);
    npc->set("attitude", "friendly");
    npc->move(room);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallFight(me, npc->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("并不想跟你较量"));

    npc->move(nullptr);
    delete npc;
}

// =====================================================================
// kill：跟随守卫/观战守卫/保镖守卫/准入规则否决/开战成功/守卫豁免规则
// =====================================================================
TEST_F(CmdStandardCombatTest, Kill_PartyFollow_Rejected)
{
    CUser* boss = MakeGateUser("镖头", "k_boss1", 80);
    boss->move(room);
    boss->delete_temp("client/loading");
    g_user_list[boss->object_id()] = boss;

    CUser* killer = MakeGateUser("随行者", "k_fol1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");
    killer->set_temp("party/master", "k_boss1");
    killer->set_temp("party/follow", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallKill(killer, target->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你正处于组队跟随模式"));

    g_user_list.erase(boss->object_id());
    boss->move(nullptr);
    delete boss;
    killer->move(nullptr);
    delete killer;
}

TEST_F(CmdStandardCombatTest, Kill_ObservingVictim_Rejected)
{
    CUser* killer = MakeGateUser("夜行人", "k_obs1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");

    CUser* victim = MakeGateUser("观战者", "k_obs_v", 80);
    victim->move(room);
    victim->delete_temp("client/loading");
    victim->set("corps/id", "k_obs_corps");
    victim->set("corps/rank", "帮众");
    victim->set_temp("fight/is_fighting", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallKill(killer, victim->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("不能在观战中暗中偷袭"));

    killer->move(nullptr);
    delete killer;
    victim->move(nullptr);
    delete victim;
}

TEST_F(CmdStandardCombatTest, Kill_BodyguardVictim_Rejected)
{
    CUser* killer = MakeGateUser("独行客", "k_bg1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");

    CUser* victim = MakeGateUser("大镖头", "k_bg_v", 80);
    victim->move(room);
    victim->delete_temp("client/loading");
    victim->set("corps/id", "k_bg_corps");
    victim->set("corps/rank", "镖头");
    victim->set_temp("保镖", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallKill(killer, victim->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("趟子手簇拥"));

    killer->move(nullptr);
    delete killer;
    victim->move(nullptr);
    delete victim;
}

TEST_F(CmdStandardCombatTest, Kill_AttackGateVeto_Rejected)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(
        luaL_dostring(L,
                      "__orig_cr = hooks.combat_rule;"
                      "hooks.combat_rule = function(phase,a,t,d)"
                      "  if phase == 'attack_gate' then return "
                      "{reason='此处不准动手'} end"
                      "  if __orig_cr then return __orig_cr(phase,a,t,d) end"
                      ";end"),
        LUA_OK);

    CUser* killer = MakeGateUser("访客", "k_veto1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");

    CUser* victim = MakeGateUser("良民", "k_veto_v", 80);
    victim->move(room);
    victim->delete_temp("client/loading");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallKill(killer, victim->object_id(), (char*)""));
    EXPECT_STREQ(g_notify_fail_msg, "此处不准动手") << "数据集规则否决出手准入";

    EXPECT_EQ(
        luaL_dostring(L, "hooks.combat_rule = __orig_cr; __orig_cr = nil"),
        LUA_OK);

    killer->move(nullptr);
    delete killer;
    victim->move(nullptr);
    delete victim;
}

TEST_F(CmdStandardCombatTest, Kill_Success_StartsBattle)
{
    g_current_time = 1000000;  // ready_pk 的 PK 标以时钟为值
    CUser* killer  = MakeGateUser("挑战者", "k_suc1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");

    CUser* victim = MakeGateUser("应战者", "k_suc_v", 80);
    victim->move(room);
    victim->delete_temp("client/loading");
    victim->set("corps/id", "k_suc_corps");
    victim->set("corps/rank", "帮众");

    EXPECT_EQ(1, CallKill(killer, victim->object_id(), (char*)""));
    EXPECT_EQ(victim->environment(), killer->environment())
        << "双方一同进入战斗房";
    EXPECT_NE(room, killer->environment());
    EXPECT_NE(0, killer->query_temp("pending/PK")) << "开战挂 PK 标";

    killer->move(nullptr);
    delete killer;
    victim->move(nullptr);
    delete victim;
}

TEST_F(CmdStandardCombatTest, Kill_CityGuardExempt_RuleVetoes)
{
    ensure_lua_env();
    lua_State* L = CLuaEngine::instance()->get_state();
    EXPECT_EQ(luaL_dostring(
                  L,
                  "__orig_cr = hooks.combat_rule;"
                  "hooks.combat_rule = function(phase,a,t,d)"
                  "  if phase == 'guard_response' then return {yes=true} end"
                  "  if __orig_cr then return __orig_cr(phase,a,t,d) end"
                  ";end"),
              LUA_OK);
    g_current_time = 1000000;
    room->set("city", 1);

    CUser* killer = MakeGateUser("闹市侠", "k_city1", 80);
    killer->move(room);
    killer->delete_temp("client/loading");

    CUser* victim = MakeGateUser("闹市客", "k_city_v", 80);
    victim->move(room);
    victim->delete_temp("client/loading");
    victim->set("corps/id", "k_city_corps");
    victim->set("corps/rank", "帮众");

    EXPECT_EQ(1, CallKill(killer, victim->object_id(), (char*)""))
        << "守卫豁免规则命中则不开哨（43/45 分支）";

    EXPECT_EQ(
        luaL_dostring(L, "hooks.combat_rule = __orig_cr; __orig_cr = nil"),
        LUA_OK);

    killer->move(nullptr);
    delete killer;
    victim->move(nullptr);
    delete victim;
}

// =====================================================================
// cast：禁法房间/未准备技能/须空手/武器类型不符
// =====================================================================
TEST_F(CmdStandardCombatTest, Cast_NoCastRoom_Rejected)
{
    room->set("no_cast", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallCast(me, 0, (char*)"test_a strike"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止使用法术"));
}

TEST_F(CmdStandardCombatTest, Cast_Unmapped_NotPrepared)
{
    me->set_skill("test_a", 10);  // 会而不备：非 special 技能须先 enable 映射
    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "test_a strike");
    EXPECT_EQ(0, CallCast(me, 0, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("请先准备技能"));
}

TEST_F(CmdStandardCombatTest, Cast_WeaponMismatch_RequiresBareHands)
{
    // unarmed 属徒手类，却握着剑 → 你必须空手
    me->set_skill("unarmed", 30);
    me->query_skill_map()->set("unarmed", "test_a");
    WieldSword(me, "sword");

    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "unarmed strike");
    EXPECT_EQ(0, CallCast(me, 0, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你必须空手"));
}

TEST_F(CmdStandardCombatTest, Cast_WrongWeaponType_Rejected)
{
    // blade 须持刃器，空手 → 使用的武器不对
    me->set_skill("blade", 30);
    me->query_skill_map()->set("blade", "test_a");

    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "blade slash");
    EXPECT_EQ(0, CallCast(me, 0, arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("使用的武器不对"));
}

// =====================================================================
// cast：基础技能的法术在 enable 高级技能后仍然可用
//   高级技能继承基础技能的法术（注册期继承），施法时转发回基础技能执行。
// =====================================================================
TEST_F(CmdStandardCombatTest, Cast_InheritedSneak_WorksAfterEnable)
{
    me->set_skill("throwing",
                  1);  // raw=1：query_skill 可见值为 0 → random 恒 0
    me->set_skill("throw_adv_sample", 10);
    me->map_skill("throwing", "throw_adv_sample");
    me->set("mp", (long)200);
    CItem* dart = GiveThrowing(me, 5);

    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "throwing sneak");
    EXPECT_EQ(1, CallCast(me, target->object_id(), arg))
        << "高级投掷应继承基本投掷的偷袭";
    EXPECT_EQ(4, dart->query_amount()) << "偷袭出手消耗一枚暗器";
    EXPECT_EQ(170, me->query("mp")) << "落空也耗 30 法力";
}

TEST_F(CmdStandardCombatTest, Cast_InheritedRecover_WorksAfterEnable)
{
    me->set_skill("force", 100);
    me->set_skill("force_adv_sample", 10);
    me->map_skill("force", "force_adv_sample");
    me->set("mp", (long)200);
    me->set("hp", (long)50);
    me->set("eff_hp", (long)100);

    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "force recover");
    EXPECT_EQ(1, CallCast(me, 0, arg)) << "高级内功应继承基本内功的复元术";
    EXPECT_GT(me->query("hp"), 50) << "复元术回血";
    EXPECT_LT(me->query("mp"), 200) << "复元术耗内力";
}

TEST_F(CmdStandardCombatTest, Cast_BasicMagicWithoutEnable_NotPrepared)
{
    // 未 enable 任何高级技能：基础技能的法术不可施放（须先准备技能）
    me->set_skill("throwing", 10);
    GiveThrowing(me, 5);

    g_notify_fail_msg[0] = '\0';
    char arg[64];  // analyse_string 会原地切分，不能传字面量
    snprintf(arg, sizeof(arg), "throwing sneak");
    EXPECT_EQ(0, CallCast(me, target->object_id(), arg));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("请先准备技能"));
}

// =====================================================================
// join：禁入房间/锁而未碎/不得加入/断线者砸门而入/直接入队
// =====================================================================
TEST_F(CmdStandardCombatTest, Join_NoJoinRoom_Rejected)
{
    room->set("no_join", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallJoin(me, join_fr ? join_fr->object_id() : 0, (char*)"0"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止加入战斗"));
}

TEST_F(CmdStandardCombatTest, Join_LockedShowsLockMsg_Rejected)
{
    CFightRoom* fr = MakeGuardScene("封闭战场", "join_lock");
    fr->begin_fight(1, 0);  // 阈值模式 → can_join 拒绝，走碎门判定
    fr->set("lock_msg", "铁闸封死。");
    target->move(fr);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallJoin(me, fr->object_id(), (char*)"0"));
    EXPECT_STREQ(g_notify_fail_msg, "铁闸封死。");
}

TEST_F(CmdStandardCombatTest, Join_CannotJoin_Rejected)
{
    CFightRoom* fr = MakeGuardScene("打不进门", "join_no");
    fr->begin_fight(1, 0);
    target->move(fr);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallJoin(me, fr->object_id(), (char*)"0"));
    EXPECT_STREQ(g_notify_fail_msg, "此战斗不能加入!");
}

TEST_F(CmdStandardCombatTest, Join_NetDeadFighter_SmashesAndJoins)
{
    CFightRoom* fr = MakeGuardScene("鏖战", "join_smash");
    fr->begin_fight(1, 0);
    fr->set("lock_msg", "铁闸封死。");

    CUser* downed = MakeGateUser("掉线者", "join_dead1", 50);
    downed->move(fr);
    downed->delete_temp("client/loading");
    downed->set_temp("fight/is_fighting", 1);
    downed->set_temp("net_dead", 1);

    EXPECT_EQ(1, CallJoin(me, fr->object_id(), (char*)"0"));
    EXPECT_EQ(1, fr->query("smash")) << "断线挂机者触发破门";
    EXPECT_EQ(fr, me->environment()) << "破门后得以加入";

    downed->move(nullptr);
    delete downed;
}

TEST_F(CmdStandardCombatTest, Join_Success_JoinsTeam)
{
    CFightRoom* fr = MakeGuardScene("空场", "join_ok");
    EXPECT_EQ(1, CallJoin(me, fr->object_id(), (char*)"0"));
    EXPECT_EQ(fr, me->environment()) << "入队即进入战斗房";
}

// ---------------- cmd_hit_ob 深路：战斗场入队两支 + net_dead/等级门
// ----------------

TEST_F(CmdStandardCombatTest, HitObFightRoomJoinsUntouchedOpponent)
{
    auto* fr = new CFightRoom();
    fr->set_name("fight", "hj_fight1");
    CRoom tmp;
    tmp.set_name("外场", "hj_outer");
    fr->move(&tmp);  // join_team 尾部要遍历外层环境

    auto* a = new CChar();
    a->set_name("甲", "hj_a");
    a->set("hp", (long)100);
    a->set("max_hp", (long)100);
    auto* b = new CChar();
    b->set_name("乙", "hj_b");
    b->set("id", "hj_b");
    b->set("hp", (long)100);
    b->set("max_hp", (long)100);
    a->move(fr);
    b->move(fr);
    // b is a plain CChar: present()/send chain stay visible & safe

    a->set_temp("fight/is_fighting", 1);  // 我方在战、对方未战 → 支A

    g_notify_fail_msg[0] = '\0';
    int r                = a->cmd_hit_ob(b, (char*)"");

    EXPECT_EQ(1, r) << "战斗场支返回 1(249)";
    EXPECT_EQ(1, static_cast<int>(b->query_temp("fight/fight_team")))
        << "未开战的对手被拉入对面队(233-236)";

    a->move(nullptr);
    b->move(nullptr);
    delete a;
    delete b;
    fr->move(nullptr);  // 先出房再拆
    delete fr;
}

TEST_F(CmdStandardCombatTest, HitObFightRoomJoinsSelfWhenOpponentFighting)
{
    auto* fr = new CFightRoom();
    fr->set_name("fight", "hj_fight2");
    CRoom tmp;
    tmp.set_name("外场", "hj_outer");
    fr->move(&tmp);  // join_team 尾部要遍历外层环境

    auto* a = new CChar();
    a->set_name("丙", "hj_c");
    a->set("hp", (long)100);
    a->set("max_hp", (long)100);
    auto* b = new CChar();
    b->set_name("丁", "hj_d");
    b->set("id", "hj_d");
    b->set("hp", (long)100);
    b->set("max_hp", (long)100);
    a->move(fr);
    b->move(fr);
    // b is a plain CChar: present()/send chain stay visible & safe

    b->set_temp("fight/is_fighting", 1);  // 对方在战、我方未战 → 支B

    g_notify_fail_msg[0] = '\0';
    int r                = a->cmd_hit_ob(
        b,
        (char*)"");  // 直调：绕开 kill 的 set_killer/tell_object 无 client 层

    EXPECT_EQ(1, r) << "战斗场支返回 1(249)";
    EXPECT_EQ(1, static_cast<int>(a->query_temp("fight/fight_team")))
        << "我方加入对面队(243-246)";

    a->move(nullptr);
    b->move(nullptr);
    delete a;
    delete b;
    fr->move(nullptr);  // 先出房再拆
    delete fr;
}

TEST_F(CmdStandardCombatTest, HitObNetDeadTargetRejected)
{
    auto* env = new CContainer();  // 普通房（非战斗场支）
    auto* a   = new CChar();
    a->set_name("戊", "hj_e");
    a->set("hp", (long)100);
    a->set("max_hp", (long)100);
    auto* b = new CUser();
    b->set_name("己", "hj_f");
    b->set("id", "hj_f");
    b->set("hp", (long)100);
    b->set("max_hp", (long)100);
    a->move(env);
    b->move(env);
    b->delete_temp("client/loading");  // 同上：不可见会让 get_room_char 落空

    b->set_temp("net_dead", 1);

    g_notify_fail_msg[0] = '\0';
    int r                = CallKill(a, b->object_id(), (char*)"");
    EXPECT_EQ(0, r) << "掉线者不可被挑战(255)";
    EXPECT_NE(g_notify_fail_msg[0], '\0') << "拒绝消息已给出";

    a->move(nullptr);
    b->move(nullptr);
    delete a;
    delete b;
    delete env;
}

TEST_F(CmdStandardCombatTest, HitObPkProtectLevelGate)
{
    auto* env = new CContainer();
    env->set("pk_protect", 30);
    auto* a = new CChar();
    a->set_name("庚", "hj_g");
    a->set("hp", (long)100);
    a->set("max_hp", (long)100);
    a->set("level", (long)50);
    auto* b = new CUser();
    b->set_name("辛", "hj_h");
    b->set("id", "hj_h");
    b->set("hp", (long)100);
    b->set("max_hp", (long)100);
    b->set("level", (long)10);
    a->move(env);
    b->move(env);
    b->delete_temp("client/loading");  // 同上

    g_notify_fail_msg[0] = '\0';
    int r                = CallKill(a, b->object_id(), (char*)"");
    EXPECT_EQ(0, r) << "保护区内高等级不得挑战低等级(263)";
    EXPECT_NE(g_notify_fail_msg[0], '\0') << "拒绝消息已给出";

    a->move(nullptr);
    b->move(nullptr);
    delete a;
    delete b;
    delete env;
}

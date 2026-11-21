// test_npc_behavior.cpp — CNpc 行为面：self_action 自走发言 / random_move 随机
// 走动 / 战斗态自走分支（src/npc/Npc.cpp）
//
// 关键约定：
//   - is_living = !IS_LIVING：set_temp("disabled","1") 置假（test_auto_fight
//   先例）
//   - is_fighting = query_temp("fight/is_fighting")（Char.cpp 直读 temp）
//   - random_move 经 command("go 0 <dir>")：目标房须先注册 g_room_list
//
//
#include "pch.h"

#include "Global.h"
#include "Npc.h"
#include "Room.h"
#include "test_lua_env.h"

namespace {

// 战室事实房间：仅覆写 IS_FIGHTROOM 判定（CFightRoom 直移入有入房登记约束）
class FakeFightRoom : public CRoom {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == IS_FIGHTROOM) return 1;
        return CRoom::query_attr(attr);
    }
};

// 能发言的 NPC：chat_msg 覆盖返回固定文案（基类恒 NULL）
class ChatNpc : public CNpc {
public:
    char* chat_msg() override { return const_cast<char*>("老者咕哝了一声。"); }
};

}  // namespace

class NpcBehaviorTest : public ::testing::Test {
protected:
    CRoom room;
    CRoom next_room;  // random_move 的目标房（注册进 g_room_list）
    ChatNpc npc;
    CChar listener;

    void SetUp() override
    {
        ensure_lua_env();  // do_talk → 通用对话钩子 走脚本系统

        room.set_name("演武场", "npc_practice");
        next_room.set_name("回廊", "npc_next");
        g_room_list["npc_next"] = &next_room;

        npc.set_name("老者", "elder1");
        npc.set("hp", (long)100);
        npc.set("max_hp", (long)100);
        npc.move(&room);

        listener.set_name("路人", "passer1");
        listener.move(&room);
    }

    void TearDown() override { g_room_list.erase("npc_next"); }
};

// ---------------- self_action 守卫 ----------------

TEST_F(NpcBehaviorTest, SelfActionGuardsReturnZero)
{
    ChatNpc orphan;
    orphan.set_name("流浪者", "drifter");
    orphan.set("chat_chance", (long)100);
    EXPECT_EQ(0, orphan.self_action()) << "无环境即止(35)";

    EXPECT_EQ(0, npc.self_action()) << "未配 chat_chance 即止(38)";

    npc.set("chat_chance", (long)100);
    npc.set_temp("disabled", "1");  // IS_LIVING → 0
    EXPECT_EQ(0, npc.self_action()) << "非存活即止(36)";
}

// ---------------- self_action 非战斗发言 ----------------

TEST_F(NpcBehaviorTest, SelfActionNonFightChatRollsAndEmits)
{
    npc.set("chat_chance", (long)100);
    EXPECT_EQ(1, npc.self_action()) << "满几率 → 发言支(42,52,54,56)";
    EXPECT_EQ(1, npc.self_action()) << "可重复";

    npc.set("chat_chance", (long)1);
    int zeros = 0;
    for (int i = 0; i < 2000; i++) {
        if (npc.self_action() == 0) zeros++;
    }
    EXPECT_GE(zeros, 500) << "低几率未命中返回 0(59)";
}

// ---------------- self_action 战斗态分支 ----------------

TEST_F(NpcBehaviorTest, SelfActionFightOutsideFightRoomStillWalks)
{
    npc.set_temp("fight/is_fighting", 1);
    npc.set("chat_chance_combat", (long)100);
    EXPECT_EQ(1, npc.self_action())
        << "非战室战斗 → WARN 支仍自走返回 1(43,45,48)";
}

TEST_F(NpcBehaviorTest, SelfActionFightRoomUsesCombatChat)
{
    FakeFightRoom froom;
    froom.set_name("斗室", "npc_fight");
    npc.move(&froom);
    npc.set_temp("fight/is_fighting", 1);
    npc.set("chat_chance_combat", (long)100);

    EXPECT_EQ(1, npc.self_action()) << "战室战斗 → chat_msg_combat 支(45,46)";
    npc.move(&room);  // 人先出房，避免栈房析构序问题
}

// ---------------- random_move ----------------

TEST_F(NpcBehaviorTest, RandomMoveTakesAnExit)
{
    room.add_door((char*)"north", (char*)"npc_next", (char*)"south",
                  (char*)"N");
    npc.random_move();
    EXPECT_NE(static_cast<CContainer*>(&room), npc.environment())
        << "随机取出口离开当前房(221-230)";
}

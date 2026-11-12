// test_char_fight_join.cpp - 开战时同室第三方入战的门（CChar::cmd_hit_ob）。
//
// Build+Run: .\run-coverage.ps1
//
// 开战搬房之后，cmd_hit_ob 遍历发起者原房间，按互不相干的门决定谁被搬进
// 战斗房：受害者同门派、双方杀意名单（组队跟随另在 CFightRoom::join_team）。
// 这里钉住两道门各自的实际口径：门派门只认「受害者同门派且该旁观者有门派」，
// 杀意名单门既不看门派也不分玩家/NPC。
#include "pch.h"
#include "Char.h"
#include "User.h"
#include "Room.h"
#include "FightRoom.h"
#include "HeartBeat.h"
#include "Global.h"
#include "test_lua_env.h"
#include "mock/mock_itemmgr.h"

#include <cstring>
#include <vector>

extern char g_notify_fail_msg[512];

namespace {

// 战斗精灵工厂记账：cmd_hit_ob 经 load_item("fight_room") 取用的 CFightRoom
// 由工厂 new 出并登记，用例结束统一摘心跳、脱离、销毁。
std::vector<CFightRoom*> g_join_fight_rooms;

CContainer* JoinFightRoomFactory(const char* base_name)
{
    if (base_name && strcmp(base_name, "fight_room") == 0) {
        auto* fr = new CFightRoom();
        g_join_fight_rooms.push_back(fr);
        return fr;
    }
    return nullptr;
}

CChar* MakeJoinChar(const char* name, const char* id)
{
    auto* c = new CChar();
    c->set_name(name, id);
    c->set("id", id);
    c->set("hp", (long)100);
    c->set("max_hp", (long)100);
    return c;
}

}  // namespace

class CharFightJoinTest : public ::testing::Test {
protected:
    CRoom env;
    CChar* attacker = nullptr;
    CChar* victim   = nullptr;
    std::vector<CChar*> bystanders;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        g_join_fight_rooms.clear();
        g_notify_fail_msg[0] = '\0';

        env.set_name("同室", "join_env");
        env.set("base_name", "join_env");
        env.set("max_items", 30);

        attacker = MakeJoinChar("甲", "join_atk");
        victim   = MakeJoinChar("乙", "join_vic");
        attacker->move(&env);
        victim->move(&env);

        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &JoinFightRoomFactory;
        g_item_mgr        = mgr;
    }

    void TearDown() override
    {
        attacker->move(nullptr);
        victim->move(nullptr);
        for (CChar* by : bystanders) {
            by->move(nullptr);
            delete by;
        }
        bystanders.clear();

        for (CFightRoom* fr : g_join_fight_rooms) {
            set_heart_beat_obj(fr, 0);  // begin_fight 注册过心跳
            fr->move(nullptr);
            delete fr;
        }
        g_join_fight_rooms.clear();

        delete attacker;
        delete victim;

        g_item_mgr = saved_mgr;
        delete mgr;
    }

    // 造一个同室旁观者（用例自行决定它的门派与杀意名单）
    CChar* AddBystander(const char* name, const char* id)
    {
        CChar* by = MakeJoinChar(name, id);
        by->move(&env);
        bystanders.push_back(by);
        return by;
    }

    // 开战：发起者与受害者搬进战斗房，返回该房供断言
    CFightRoom* StartFight()
    {
        EXPECT_EQ(1, attacker->cmd_hit_ob(victim, (char*)""));
        EXPECT_EQ(1u, g_join_fight_rooms.size());
        return g_join_fight_rooms.empty() ? nullptr : g_join_fight_rooms[0];
    }
};

// 无门派旁观者不入战：门派门的 bangpai[0] 守卫挡掉空门派，空门派不与空门派互认
TEST_F(CharFightJoinTest, FactionlessBystanderStaysInRoom)
{
    CChar* by = AddBystander("丙", "join_by1");

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(static_cast<CContainer*>(&env), by->environment())
        << "无门派旁观者留在原房间";
}

// 受害者同门派的旁观者入战，且站在受害者一队（join <房> 1 → B 队）
TEST_F(CharFightJoinTest, VictimFamilyBystanderJoinsVictimSide)
{
    victim->set("family/family_name", "南国");
    CChar* by = AddBystander("丁", "join_by2");
    by->set("family/family_name", "南国");

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(fr, by->environment()) << "受害者同门派旁观者被搬进战斗房";
    EXPECT_EQ(1, static_cast<int>(by->query_temp("fight/fight_team")))
        << "站在受害者一队";
}

// 杀意名单门不看门派：对攻方有账的无门派旁观者照样入战，站受害者一队
TEST_F(CharFightJoinTest, GrudgeOnAttackerJoinsVictimSide)
{
    CChar* by = AddBystander("戊", "join_by3");
    by->set_killer(attacker);

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(fr, by->environment()) << "名单上有攻方 → 被搬进战斗房";
    EXPECT_EQ(1, static_cast<int>(by->query_temp("fight/fight_team")))
        << "敌人的敌人站受害者一队";
}

// 杀意名单门的另一支：对受害者有账 → 站攻方一队
TEST_F(CharFightJoinTest, GrudgeOnVictimJoinsAttackerSide)
{
    CChar* by = AddBystander("己", "join_by4");
    by->set_killer(victim);

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(fr, by->environment()) << "名单上有受害者 → 被搬进战斗房";
    EXPECT_EQ(0, static_cast<int>(by->query_temp("fight/fight_team")))
        << "对受害者有账 → 站攻方一队";
}

// 杀意名单门不分玩家与 NPC：玩家旁观者同样被 command("join") 发起入队，
// 但站进的是受害者（NPC）那一队 → 被「禁止加入 NPC 一方」拦在房外
TEST_F(CharFightJoinTest, GrudgeUserBystanderBlockedFromNpcSide)
{
    auto* by = new CUser();
    by->set_name("游客", "join_by5");
    by->set("id", "join_by5");
    by->set("hp", (long)100);
    by->set("max_hp", (long)100);
    by->move(&env);
    bystanders.push_back(by);           // 先登记，早退也不漏回收
    by->delete_temp("client/loading");  // 进房挂的加载回执会静默吞掉命令
    by->set_killer(attacker);

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(static_cast<CContainer*>(&env), by->environment())
        << "名单门放行了玩家，但 NPC 一侧的门把它挡回原房间";
    EXPECT_STREQ(g_notify_fail_msg, "此战斗禁止加入NPC一方!")
        << "拦下它的是入队门，不是名单门没触发";
}

// 玩家旁观者站的不是 NPC 一侧时不受该门限制：对受害者有账 → 跟攻方同队入战
TEST_F(CharFightJoinTest, GrudgeUserBystanderJoinsAttackerSide)
{
    auto* by = new CUser();
    by->set_name("游客", "join_by6");
    by->set("id", "join_by6");
    by->set("hp", (long)100);
    by->set("max_hp", (long)100);
    by->move(&env);
    bystanders.push_back(by);
    by->delete_temp("client/loading");  // 进房挂的加载回执会静默吞掉命令
    by->set_killer(victim);

    CFightRoom* fr = StartFight();
    ASSERT_NE(nullptr, fr);

    EXPECT_EQ(fr, by->environment()) << "站攻方一队的玩家旁观者入战";
    EXPECT_EQ(0, static_cast<int>(by->query_temp("fight/fight_team")));
}

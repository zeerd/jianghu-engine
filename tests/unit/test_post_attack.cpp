// test_post_attack.cpp — 宠物命中后主人奖励（驯兽术/驱蜂）的行为基线
//
// 驱动 CSetEffect::notify(PostAttack) 走真实 dataset hooks.set_effect，
// 锁定"宠物命中 → 在线主人获得技能进步 + 战斗经验"的可观察行为，
// 覆盖其全部守卫分支：
//   damage>0、master_id 指向在线主人、宠物具备 xunshou_shu 或 qufeng、
//   宠物与猎物等级差 < 10。
// 观察面：master 的 query_learned()（weak_mode=1 + is_user 只累加 learned）
//        与 query("combat_exp")。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include "Attack.h"
#include "Char.h"
#include "Container.h"
#include "SetEffect.h"
#include "Skill.h"
#include "SkillMgr.h"  // skill_release_one
#include "User.h"
#include "test_lua_env.h"

extern MAP2USER g_user_list;
extern MAP2SKILL g_skill_list;

class PostAttackTest : public ::testing::Test {
protected:
    CContainer room;
    CChar pet;
    CChar victim;
    CUser* master = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("room", "pa_room");
        pet.set_name("宠物", "pet1");
        victim.set_name("猎物", "vic1");
        pet.set("level", (long)5);
        victim.set("level", (long)5);  // abs_lvl = 0 < 10
        pet.move(&room);
        victim.move(&room);

        master = new CUser();
        master->set_name("主人", "m1");
        master->set("id", "m1");
        master->set("level", (long)20);  // level2skill(20)=69
        master->move(&room);
        g_user_list[master->object_id()] = master;
    }

    void TearDown() override
    {
        g_user_list.erase(master->object_id());
        master->move(nullptr);
        delete master;
        pet.move(nullptr);
        victim.move(nullptr);
        room.move(nullptr);
        // 摘除用例经 improve_skill/set_skill 落入全局技能缓存的脚本名技能
        // （连带摘除继承自它们的技能：继承项的归属指针不能悬垂）
        for (const char* name : {"xunshou_shu", "beekeeping"}) {
            skill_release_one(name);
        }
    }

    // 使宠物成为"有主人的驯兽/驱蜂宠物"
    void MakeTamed(bool xunshou, bool qufeng)
    {
        pet.set("master_id", "m1");
        if (xunshou) pet.set("xunshou_shu", (long)1);
        if (qufeng) pet.set("qufeng", (long)1);
    }

    // 触发一次攻击结算后置通知（宠物命中奖励在此派发）
    void Run(int damage)
    {
        AttackContext ctx;
        ctx.me     = &pet;
        ctx.victim = &victim;
        ctx.damage = damage;
        CSetEffect::notify(ctx, SetEffectPhase::PostAttack);
    }
};

// ====================================================================
// 守卫：damage<=0 不奖励
// ====================================================================
TEST_F(PostAttackTest, ZeroDamage_NoReward)
{
    MakeTamed(true, false);
    Run(/*damage=*/0);
    EXPECT_EQ(master->query_learned()->query("xunshou_shu"), 0);
    EXPECT_EQ(master->query("combat_exp"), 0);
}

// ====================================================================
// 守卫：无 master_id 不奖励
// ====================================================================
TEST_F(PostAttackTest, NoMasterId_NoReward)
{
    pet.set("xunshou_shu", (long)1);  // 有驯兽标志但没有主人
    Run(/*damage=*/10);
    EXPECT_EQ(master->query_learned()->query("xunshou_shu"), 0);
    EXPECT_EQ(master->query("combat_exp"), 0);
}

// ====================================================================
// 守卫：主人不在线不奖励
// ====================================================================
TEST_F(PostAttackTest, MasterOffline_NoReward)
{
    MakeTamed(true, false);
    g_user_list.erase(master->object_id());  // 让主人下线
    Run(/*damage=*/10);
    EXPECT_EQ(master->query_learned()->query("xunshou_shu"), 0);
    EXPECT_EQ(master->query("combat_exp"), 0);
}

// ====================================================================
// 守卫：宠物与猎物等级差 >= 10 不奖励
// ====================================================================
TEST_F(PostAttackTest, LevelGap_NoReward)
{
    MakeTamed(true, false);
    victim.set("level", (long)-10);  // abs_lvl = 5 - (-10) = 15 >= 10
    Run(/*damage=*/10);
    EXPECT_EQ(master->query_learned()->query("xunshou_shu"), 0);
    EXPECT_EQ(master->query("combat_exp"), 0);
}

// ====================================================================
// 守卫：普通宠物（无 xunshou_shu 且无 qufeng）不奖励
// ====================================================================
TEST_F(PostAttackTest, PlainPet_NoReward)
{
    pet.set("master_id", "m1");  // 有主人，但没有驯兽/驱蜂标志
    Run(/*damage=*/10);
    EXPECT_EQ(master->query_learned()->query("xunshou_shu"), 0);
    EXPECT_EQ(master->query_learned()->query("beekeeping"), 0);
    EXPECT_EQ(master->query("combat_exp"), 0);
}

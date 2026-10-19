// test_corpse.cpp — CCorpse：属性判定、心跳腐烂与尸体制造
//
// 契约：
//  - query_attr 对腐烂相关属性给出确定答案，未列明的属性回落 CChar
//  - heart_beat 按 m_delay 倒数，倒数归零才腐烂一次
//  - decay 三阶段：腐烂 → 骸骨 → 化灰（遗物归还房间并自毁）
//  - make_corpse 承接遇害者姓名与属性；非主人随身物留在房间，
//    巫师装备随身保留
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>

#include "Char.h"
#include "Corpse.h"
#include "Global.h"
#include "Item.h"
#include "test_lua_env.h"
#include "mock/mock_itemmgr.h"

namespace {

// 与 CItemMgr::load_item 同序：先写基名再 create
CContainer* corpse_factory(const char* base_name)
{
    if (std::strcmp(base_name, "corpse_ob") != 0) return nullptr;
    CCorpse* corpse = new CCorpse();
    corpse->set("base_name", base_name);
    corpse->create();
    return corpse;
}

class CorpseTest : public ::testing::Test {
protected:
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;
    CContainer env;

    void SetUp() override
    {
        ensure_lua_env();
        // 测试房间不设负重上限，角色与装备才能移入
        env.set("infinity", 1);
        saved_mgr             = g_item_mgr;
        item_mgr.load_factory = corpse_factory;
        g_item_mgr            = &item_mgr;
    }

    void TearDown() override { g_item_mgr = saved_mgr; }

    // 尸体构造即挂全局心跳：销毁前注销，避免心跳表悬挂
    static void FreeCorpse(CCorpse* corpse)
    {
        corpse->set_heart_beat(0);
        corpse->move(nullptr);
        delete corpse;
    }

    static void FreeChar(CChar* ob)
    {
        ob->set_heart_beat(0);
        ob->move(nullptr);
        delete ob;
    }

    // 把尸体推到第 stage 次腐烂（stage 1..3）
    static void DecayTo(CCorpse* corpse, int stage)
    {
        int rounds = stage == 1 ? 121 : (stage == 2 ? 121 : 61);
        for (int i = 0; i < rounds; i++) corpse->heart_beat();
    }
};

}  // namespace

// 构造：默认名与腐烂属性判定，未列明属性回落 CChar
TEST_F(CorpseTest, Constructor_SetsDefaultsAndQueryAttrFallsBack)
{
    CCorpse corpse;
    corpse.set_heart_beat(0);

    EXPECT_STREQ(corpse.name(), "无名尸体");
    EXPECT_EQ(corpse.query_attr(CMainObject::IS_CORPSE), 1);
    EXPECT_EQ(corpse.query_attr(CMainObject::IS_LIVING), 0);
    EXPECT_EQ(corpse.query_attr(CMainObject::IS_CHAR), 0);
    EXPECT_EQ(corpse.query_attr(CMainObject::CAN_GET), 1);
    EXPECT_EQ(corpse.query_attr(CChar::IS_USER), 0);
}

// 心跳倒数归零才腐烂一次，腐烂后重置倒数
TEST_F(CorpseTest, HeartBeat_CountsDownThenDecaysOnce)
{
    CCorpse* corpse = new CCorpse();
    corpse->set_heart_beat(0);
    corpse->move(&env);

    for (int i = 0; i < 120; i++) corpse->heart_beat();
    EXPECT_STREQ(corpse->name(), "无名尸体") << "倒数未归零不得腐烂";

    corpse->heart_beat();
    EXPECT_STREQ(corpse->name(), "腐烂的尸体");
    EXPECT_STREQ(corpse->querystr("base_name"), "corpse_ob");

    for (int i = 0; i < 120; i++) corpse->heart_beat();
    EXPECT_STREQ(corpse->name(), "腐烂的尸体") << "倒数未归零不得再次腐烂";

    FreeCorpse(corpse);
}

// 第三阶段：遗物归还房间，尸体自毁
TEST_F(CorpseTest, ThirdStage_ReturnsLootAndDestroysCorpse)
{
    CCorpse* corpse = new CCorpse();
    corpse->set_heart_beat(0);
    corpse->move(&env);

    CItem* loot = new CItem();
    loot->set_name("遗物", "corpse_loot");
    loot->move(corpse);

    const int32_t corpse_id = corpse->object_id();

    DecayTo(corpse, 1);
    EXPECT_STREQ(corpse->name(), "腐烂的尸体");
    DecayTo(corpse, 2);
    EXPECT_STREQ(corpse->name(), "一具枯干的骸骨");

    DecayTo(corpse, 3);
    EXPECT_EQ(g_last_destruct_object, corpse_id) << "化灰必须自毁";
    EXPECT_EQ(loot->environment(), &env) << "遗物必须归还房间";

    loot->move(nullptr);
    delete loot;
}

// 制造尸体：承接遇害者姓名与属性，非主人随身物留在房间
TEST_F(CorpseTest, MakeCorpse_CopiesVictimAndDropsForeignLoot)
{
    CChar* victim = new CChar();
    victim->set_name("遇害者", "corpse_victim");
    victim->set("age", (long)33);
    victim->set("gender", "男性");
    victim->set_weight(1234);
    victim->move(&env);

    CChar* killer = new CChar();
    killer->set_name("凶手", "corpse_killer");
    killer->move(&env);

    CItem* gear = new CItem();
    gear->set_name("佩剑", "corpse_gear");
    gear->move(victim);

    CCorpse* corpse = CCorpse::make_corpse(victim, killer);
    ASSERT_NE(corpse, nullptr);
    EXPECT_EQ(corpse->environment(), &env) << "尸体必须落在遇害者的房间";
    EXPECT_TRUE(std::strstr(corpse->name(), "遇害者") != nullptr)
        << corpse->name();
    EXPECT_EQ(corpse->query("age"), 33);
    EXPECT_EQ(corpse->query_weight(), victim->query_weight());
    EXPECT_EQ(gear->environment(), &env) << "非主人随身物留在房间";

    FreeCorpse(corpse);
    gear->move(nullptr);
    delete gear;
    FreeChar(killer);
    FreeChar(victim);
}

// 制造尸体：巫师的随身装备不被剥离
TEST_F(CorpseTest, MakeCorpse_WizardKeepsEquipment)
{
    CChar* victim = new CChar();
    victim->set_name("遇害巫师", "corpse_wizard");
    victim->set("grant", (long)CChar::GRANT_WIZARD);
    victim->move(&env);

    CChar* killer = new CChar();
    killer->set_name("凶手", "corpse_killer2");
    killer->move(&env);

    CItem* gear = new CItem();
    gear->set_name("拂尘", "corpse_staff");
    gear->move(victim);

    CCorpse* corpse = CCorpse::make_corpse(victim, killer);
    ASSERT_NE(corpse, nullptr);
    EXPECT_EQ(gear->environment(), victim) << "巫师装备随身保留";

    FreeCorpse(corpse);
    gear->move(nullptr);
    delete gear;
    FreeChar(killer);
    FreeChar(victim);
}

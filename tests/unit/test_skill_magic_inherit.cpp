// test_skill_magic_inherit.cpp - 技能法术继承契约
//
// 背景：高级技能 enable 到某技能种类后，基础技能自带的法术仍要可用。
//       引擎在技能 create 收尾时，把「可 enable 的技能种类」对应基础技能的
//       法术登记进本技能（同名以本技能自注册的为准），施法时按归属技能转发。
//
// 本文件覆盖：
//   - 继承登记：高级技能的法术表含基础技能法术，归属指向基础技能；
//   - 转发执行：施放继承来的法术时，执行的是基础技能脚本里的实现；
//   - 同名优先：本技能自注册的法术压过继承来的同名法术；
//   - 无幽灵法术：基础技能没有法术的种类不产生空法术条目。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>
#include <vector>

#include "Char.h"
#include "Container.h"
#include "FightRoom.h"
#include "Global.h"  // g_skill_list
#include "Item.h"
#include "LuaSkill.h"  // CLuaSkill
#include "Skill.h"
#include "SkillMgr.h"  // load_skill / skill_release_one
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];

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

// 未开战命中会 kill_ob 开战——经 g_item_mgr 造真实战室（默认空指针）；
// 战室登记点由夹具统一回收
std::vector<CContainer*> g_created_fight_rooms;

CContainer* inherit_item_factory(const char* base_name)
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

class SkillMagicInheritTest : public ::testing::Test {
protected:
    CContainer* room    = nullptr;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;
    MockDatabase* db    = nullptr;
    std::vector<CContainer*> m_items;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        db                = new MockDatabase();
        g_db              = db;
        saved_mgr         = g_item_mgr;
        mgr               = new CMockItemMgr();
        mgr->load_factory = &inherit_item_factory;
        g_item_mgr        = mgr;
        room              = new CContainer();
        room->set_name("竹林", "inherit_room");
        room->set("infinity", 1);
    }

    void TearDown() override
    {
        for (auto* it : m_items) {
            it->set_heart_beat(0);
            it->move(nullptr);
        }
        m_items.clear();
        for (auto* fr : g_created_fight_rooms) delete fr;
        g_created_fight_rooms.clear();
        delete room;
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr  = nullptr;
        g_db = nullptr;
        delete db;
        db = nullptr;
    }

    // 偷袭者：会基本投掷、法力充沛
    CChar* MakeSneaker(const char* id)
    {
        CChar* me = new CChar();
        me->set_name("镖师", id);
        me->set("mp", (long)1000);
        me->set("hp", (long)100);
        me->set("eff_hp", (long)100);
        me->set_skill("throwing", 100);
        me->move(room);
        m_items.push_back(me);
        return me;
    }

    CChar* MakeTarget(const char* id)
    {
        CChar* c = new CChar();
        c->set_name("路人", id);
        c->set("hp", (long)100);
        c->set("eff_hp", (long)100);
        c->move(room);
        m_items.push_back(c);
        return c;
    }

    // 造一枚暗器放入身上
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
};

// =====================================================================
// 继承登记：高级技能的法术表含基础技能法术，归属指向基础技能
// =====================================================================
TEST_F(SkillMagicInheritTest, Inherit_RegistersBasicSkillMagic)
{
    CLuaSkill adv("inherit_sample");
    adv.create();

    EXPECT_TRUE(adv.has_magic("sneak")) << "高级技能应继承到基本投掷的「偷袭」";
    EXPECT_EQ(1, adv.get_magic_count())
        << "继承后法术表只应有继承来的「偷袭」一项";

    CSkill* owner = adv.magic_owner("sneak");
    ASSERT_NE(nullptr, owner) << "继承来的法术必须有归属技能";
    EXPECT_STREQ("throwing", owner->querystr("base_name"))
        << "归属技能应是基本投掷（法术逻辑留在基础技能脚本里）";
    EXPECT_NE(&adv, owner) << "继承来的法术归属不是本技能";
}

// =====================================================================
// 转发执行：施放继承来的法术时，跑的是基础技能脚本里的实现
// =====================================================================
TEST_F(SkillMagicInheritTest, Inherit_CastRunsBasicSkillScript)
{
    CLuaSkill adv("inherit_sample");
    adv.create();

    CChar* me   = MakeSneaker("inh1");
    CChar* targ = MakeTarget("inh1_t");

    // 身上没有暗器 → 基础投掷脚本的 do_sneak 守卫生效
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, adv.cast("sneak", me, targ));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("没有暗器"))
        << "继承来的法术必须执行基础投掷脚本里的实现";
}

TEST_F(SkillMagicInheritTest, Inherit_CastConsumesWeapon)
{
    CLuaSkill adv("inherit_sample");
    adv.create();

    CChar* me   = MakeSneaker("inh2");
    CChar* targ = MakeTarget("inh2_t");
    CItem* dart = GiveThrowing(me, 5);

    me->set_skill("throwing", 1);  // random(1) 恒 0：必落空（不打伤害）
    me->set("mp", (long)200);

    EXPECT_EQ(1, adv.cast("sneak", me, targ));
    EXPECT_EQ(4, dart->query_amount()) << "每出手消耗一枚暗器";
    EXPECT_EQ(170, me->query("mp")) << "落空也耗 30 法力";
}

// =====================================================================
// 同名优先：本技能自注册的法术压过继承来的同名法术
// =====================================================================
TEST_F(SkillMagicInheritTest, LocalMagic_OverridesInheritedSameName)
{
    CLuaSkill adv("inherit_override_sample");
    adv.create();

    EXPECT_TRUE(adv.has_magic("sneak"));
    EXPECT_EQ(&adv, adv.magic_owner("sneak"))
        << "自注册的同名法术归属本技能，不被继承项覆盖";

    CChar* me   = MakeSneaker("inh3");
    CChar* targ = MakeTarget("inh3_t");

    // 自带的 do_sneak 恒返回 2：证明跑的是本脚本的实现而非基本投掷的
    EXPECT_EQ(2, adv.cast("sneak", me, targ));
}

// =====================================================================
// 无幽灵法术：基础技能没有法术的种类不产生空法术条目
// =====================================================================
TEST_F(SkillMagicInheritTest, NoGhostMagic_WhenBasicSkillHasNone)
{
    CLuaSkill adv("inherit_empty_sample");
    adv.create();

    EXPECT_EQ(0, adv.get_magic_count()) << "基本拳脚没有法术，不应产生继承条目";
    EXPECT_FALSE(adv.has_magic("sneak"));
    EXPECT_EQ(nullptr, adv.magic_owner("sneak")) << "未注册法术没有归属技能";
}

// =====================================================================
// 基础技能被摘除时，继承者必须一起重建
//   继承项的归属指针指向基础技能：基础技能被销毁后若继承者还留在缓存里，
//   施法转发就会踩到已释放的对象。
// =====================================================================
TEST_F(SkillMagicInheritTest, ReleaseBasicSkill_RebuildsDependents)
{
    // 继承者先进缓存（继承基本投掷的偷袭）
    CSkill* adv = load_skill("inherit_sample");
    ASSERT_NE(nullptr, adv);
    EXPECT_TRUE(adv->has_magic("sneak"));

    // 摘除基础技能：连带摘除继承者
    skill_release_one("throwing");
    EXPECT_TRUE(g_skill_list.find("throwing") == g_skill_list.end())
        << "基础技能应被摘除";
    EXPECT_TRUE(g_skill_list.find("inherit_sample") == g_skill_list.end())
        << "继承者应被连带摘除（否则继承项的归属指针悬垂）";

    // 再次施法：重建后的继承关系仍然可用
    CChar* me   = MakeSneaker("inh4");
    CChar* targ = MakeTarget("inh4_t");

    CSkill* rebuilt = load_skill("inherit_sample");
    ASSERT_NE(nullptr, rebuilt);
    EXPECT_TRUE(rebuilt->has_magic("sneak")) << "重建后应重新继承到偷袭";
    EXPECT_EQ(0, rebuilt->cast("sneak", me, targ))
        << "重建后的继承法术仍走基础投掷脚本";
}

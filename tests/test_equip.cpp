// test_equip.cpp — CEquip 装备/卸装与属性修正的单元测试
//
// 守护契约：
//   - can_equip 的三重限制（female_only / equip_lvl / 必须有 apply）；
//   - do_wield 的位置分配（双手互备、副位回退、位置占用/无位置/非主人拒绝）
//     与 apply 修正的施加；
//   - do_un_wield 撤销装备位与 apply 修正；
//   - do_action 的环境守卫（无环境记日志返回 0）与向 CItem 的回退；
//   - l_do_wield / l_do_unwield 对 CLuaEquip 的真实分发。
//
// 夹具范式（见 unit-test.instructions / testing-facts）：自装并恢复
// g_item_mgr、 房间 set("infinity",1)、清理顺序先 move(nullptr) 再 delete。

#include "pch.h"
#include <windows.h>
#include <rpc.h>  // LuaItem.inl 的 UUID 自动生成需要 RpcStringFreeA
#include "test_bind_common.h"  // BindTest（lua 环境 + call 助手）+ ItemFactory
#include "Equip.h"
#include "LuaItem.h"
#include "Mapping.h"

class EquipTest : public BindTest {
protected:
    CChar* me        = nullptr;
    CContainer* room = nullptr;
    CEquip* eq       = nullptr;
    CMockItemMgr mgr;
    IItemMgr* saved_mgr = nullptr;

    void SetUp() override
    {
        BindTest::SetUp();
        saved_mgr  = g_item_mgr;
        g_item_mgr = &mgr;

        room = new CContainer();
        room->set("infinity", (int32_t)1);
        room->set("name", "equip_room");

        me = new CChar();
        me->set("id", "tester1");
        me->set("name", "测试者");
        me->set("level", (int32_t)50);
        me->move(room);

        eq = new CEquip();
        eq->set_name("铁剑", "equip_sword");
    }

    void TearDown() override
    {
        if (eq) {
            eq->move(nullptr);
            delete eq;
            eq = nullptr;
        }
        if (me) {
            me->move(nullptr);
            delete me;
            me = nullptr;
        }
        if (room) {
            room->move(nullptr);
            delete room;
            room = nullptr;
        }
        g_item_mgr = saved_mgr;
        BindTest::TearDown();
    }

    // 装备修正：数值（damage）+ 字符串（desc），覆盖 apply 循环两条分支
    void SetApply()
    {
        CMapping apply;
        apply.set("damage", 5L);
        apply.set("desc", "锋利");
        eq->set("apply", apply);
    }

    // 装备到右手（默认 WIELD_HAND → 右手优先）
    void WieldToHand()
    {
        SetApply();
        eq->set("wield_position", (int32_t)WIELD_HAND);
        eq->move(me);
        ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    }
};

// ---------------- can_equip ----------------

TEST_F(EquipTest, CanEquip_FemaleOnlyBlocked)
{
    SetApply();
    eq->set("female_only", (int32_t)1);
    me->set("gender", "男性");
    EXPECT_EQ(eq->can_equip(me), 0);

    me->set("gender", "女性");
    EXPECT_EQ(eq->can_equip(me), 1);
}

TEST_F(EquipTest, CanEquip_LevelTooLow)
{
    SetApply();
    eq->set("equip_lvl", (int32_t)100);
    EXPECT_EQ(eq->can_equip(me), 0);
}

TEST_F(EquipTest, CanEquip_RequiresApply)
{
    // 无 apply 的装饰品不可装备
    EXPECT_EQ(eq->can_equip(me), 0);
    SetApply();
    EXPECT_EQ(eq->can_equip(me), 1);
}

// ---------------- query_attr ----------------

TEST_F(EquipTest, QueryAttr_IsEquip)
{
    EXPECT_EQ(eq->query_attr(CMainObject::IS_EQUIP), 1);
}

TEST_F(EquipTest, QueryAttr_CanEquippedNeedsCharEnvironment)
{
    SetApply();
    // 未入任何环境 → 0
    EXPECT_EQ(eq->query_attr(CMainObject::CAN_EQUIPPED), 0);

    // 在普通容器（非角色）→ 0
    eq->move(room);
    EXPECT_EQ(eq->query_attr(CMainObject::CAN_EQUIPPED), 0);

    // 在角色身上且限制满足 → 1
    eq->move(me);
    EXPECT_EQ(eq->query_attr(CMainObject::CAN_EQUIPPED), 1);

    // 无 apply → 0（del 移除键后 querymap 返回 null）
    eq->del("apply");
    EXPECT_EQ(eq->querymap("apply"), (CMapping*)nullptr);
    EXPECT_EQ(eq->query_attr(CMainObject::CAN_EQUIPPED), 0);
}

// ---------------- do_wield ----------------

TEST_F(EquipTest, Wield_SuccessAppliesPositionAndMods)
{
    WieldToHand();
    // 装备位记录在物品与角色两侧
    EXPECT_EQ(eq->query("equipped"), WIELD_RIGHT_HAND);
    EXPECT_EQ(me->query_equip(WIELD_RIGHT_HAND), eq->object_id());
    // 数值修正已加到角色
    EXPECT_EQ(me->query_temp("apply/damage"), 5);
}

TEST_F(EquipTest, Wield_TwiceSecondRejected)
{
    WieldToHand();
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
}

TEST_F(EquipTest, Wield_NoPositionRejected)
{
    // wield_position 缺省为 0（WIELD_NONE），apply 满足门禁后被位置检查拒绝
    SetApply();
    eq->move(me);
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
    EXPECT_EQ(eq->query("equipped"), 0);
}

TEST_F(EquipTest, Wield_WrongOwnerRejected)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_HAND);
    eq->set("owner", "someone_else");
    eq->move(me);
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
    EXPECT_EQ(eq->query("equipped"), 0);
}

TEST_F(EquipTest, Wield_FallsBackToSecondaryHand)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_HAND);
    // 右手已被占用 → 落到左手
    (*me->get_equip_list())[WIELD_RIGHT_HAND] = 999;
    eq->move(me);
    ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), WIELD_LEFT_HAND);
    EXPECT_EQ(me->query_equip(WIELD_LEFT_HAND), eq->object_id());
}

TEST_F(EquipTest, Wield_FallsBackToSecondaryPosition)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_HAND);
    eq->set("wield_secondary_position", (int32_t)WIELD_NECK);
    // 双手全占 → 落到副位颈部
    (*me->get_equip_list())[WIELD_RIGHT_HAND] = 999;
    (*me->get_equip_list())[WIELD_LEFT_HAND]  = 998;
    eq->move(me);
    ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), WIELD_NECK);
}

TEST_F(EquipTest, Wield_AllPositionsOccupiedRejected)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_HAND);
    eq->set("wield_secondary_position", (int32_t)WIELD_NECK);
    (*me->get_equip_list())[WIELD_RIGHT_HAND] = 999;
    (*me->get_equip_list())[WIELD_LEFT_HAND]  = 998;
    (*me->get_equip_list())[WIELD_NECK]       = 997;
    eq->move(me);
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
    EXPECT_EQ(eq->query("equipped"), 0);
}

TEST_F(EquipTest, Wield_ErrPositionSpawnsBothEars)
{
    // WIELD_ERR 是"耳"的双侧统称 → 优先右耳
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_ERR);
    eq->move(me);
    ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), WIELD_RIGHT_ERR);
}

TEST_F(EquipTest, Wield_WristPositionSpawnsRightWrist)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_WRIST);
    eq->move(me);
    ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), WIELD_RIGHT_WRIST);
}

TEST_F(EquipTest, Wield_SinglePositionDirect)
{
    // 非双手/双耳/双腕统称位：直接占用该位
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_ARMOR);
    eq->move(me);
    ASSERT_EQ(eq->do_action(CChar::DO_WIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), WIELD_ARMOR);
}

// ---------------- do_un_wield ----------------

TEST_F(EquipTest, Unwield_ClearsPositionAndMods)
{
    WieldToHand();
    ASSERT_EQ(eq->do_action(CChar::DO_UNWIELD, me), 1);
    EXPECT_EQ(eq->query("equipped"), 0);
    EXPECT_EQ(me->query_equip(WIELD_RIGHT_HAND), 0);
    // 数值修正已撤
    EXPECT_EQ(me->query_temp("apply/damage"), 0);
}

TEST_F(EquipTest, Unwield_NotEquippedRejected)
{
    SetApply();
    eq->set("wield_position", (int32_t)WIELD_HAND);
    eq->move(me);
    EXPECT_EQ(eq->do_action(CChar::DO_UNWIELD, me), 0);
}

// ---------------- do_action 环境守卫与回退 ----------------

TEST_F(EquipTest, DoAction_NoEnvironmentLoggedAndRejected)
{
    // 未入环境：记日志并返回 0，不进入 wield 分支
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
    EXPECT_EQ(eq->do_action(CChar::DO_UNWIELD, me), 0);
}

TEST_F(EquipTest, DoAction_WieldGateNeedsCanEquipped)
{
    // 无 apply → CAN_EQUIPPED 不过 → 回退 CItem::do_action → 0
    eq->set("wield_position", (int32_t)WIELD_HAND);
    eq->move(me);
    EXPECT_EQ(eq->do_action(CChar::DO_WIELD, me), 0);
}

TEST_F(EquipTest, DoAction_UnknownActFallsBackToItem)
{
    WieldToHand();
    EXPECT_EQ(eq->do_action(999, me), 0);
}

// ---------------- l_do_wield / l_do_unwield 绑定 ----------------

TEST_F(EquipTest, BindDoWield_DispatchesToCLuaEquip)
{
    // CLuaEquip 走通 l_do_wield 的 e 分支（非 equip 守卫已被
    // test_bind_item.DoWieldUnwieldGuard 覆盖）
    CLuaEquip lua_eq("equip_bind_script");
    SetApply();
    lua_eq.set("wield_position", (int32_t)WIELD_HAND);
    lua_eq.set_name("长鞭", "equip_bind_script");
    lua_eq.move(me);

    lua_bind_push_object(L, &lua_eq);
    EXPECT_EQ(CallInt(l_do_wield, 1), 1);
    EXPECT_EQ(lua_eq.query("equipped"), WIELD_RIGHT_HAND);

    lua_bind_push_object(L, &lua_eq);
    EXPECT_EQ(CallInt(l_do_unwield, 1), 1);
    EXPECT_EQ(lua_eq.query("equipped"), 0);

    lua_eq.move(nullptr);
}

TEST_F(EquipTest, BindDoWield_RejectsNonCharEnvironment)
{
    // CLuaEquip 在普通容器里：do_wield 首行 IS_CHAR 检查拒绝（返回 0
    // 而非崩溃），证明绑定直达 CEquip 内部守卫。
    CLuaEquip lua_eq("equip_bind_script2");
    SetApply();
    lua_eq.set("wield_position", (int32_t)WIELD_HAND);
    lua_eq.move(room);

    lua_bind_push_object(L, &lua_eq);
    EXPECT_EQ(CallInt(l_do_wield, 1), 0);
    EXPECT_EQ(lua_eq.query("equipped"), 0);

    lua_eq.move(nullptr);
}

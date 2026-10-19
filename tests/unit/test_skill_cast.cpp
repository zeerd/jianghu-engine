// test_skill_cast.cpp - 施法路径契约：AP 消耗与受术者状态刷新
//
// 背景：法术的 AP 耗费（Magic_t::ap，随技能等级与角色等级递减）与
//       "施法后刷新受术者状态"原先只在 C++ 法术通道里实现；法术 Lua 化后
//       Lua 分派绕过了这两步。本文件锁定 Lua 施法路径上的这两个契约。
//
// 夹具：pk_gate_sample（special 技能）
//   - 扑击 strike：CAST_TO_OTHER，敌意，ap=100
//   - 扶持 mend  ：CAST_TO_OTHER，非敌意，ap=100
//   - 凝神 focus ：CAST_TO_NONE，ap=100
// 施法者等级 50、技能等级 100 → 实际 AP 耗费
//   100 * (120 - 100/4 - 50/2) / 100 = 70
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "LuaSkill.h"  // CLuaSkill
#include "Skill.h"
#include "User.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {
// 该夹具的 AP 耗费（见文件头推导）
const int32_t EXPECTED_AP = 70;

}  // namespace

class SkillCastTest : public ::testing::Test {
protected:
    CContainer* room           = nullptr;
    StateRecordingUser* caster = nullptr;
    StateRecordingUser* target = nullptr;
    CLuaSkill* skill           = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        g_notify_fail_msg[0] = '\0';

        room = new CContainer();
        room->set_name("演武场", "cast_test_room");
        room->set("infinity", 1);

        caster = MakeUser("施法者", "caster1");
        caster->set_skill("pk_gate_sample", 100);

        target = MakeUser("受术者", "target1");

        skill = new CLuaSkill("pk_gate_sample");
        skill->set("base_name", "pk_gate_sample");
        skill->create();
    }

    void TearDown() override
    {
        delete skill;
        skill = nullptr;
        caster->move(nullptr);
        g_user_list.erase(caster->object_id());
        delete caster;
        target->move(nullptr);
        g_user_list.erase(target->object_id());
        delete target;
        delete room;
    }

    // 玩家视角的角色：有客户端映射（send_state 才不是空操作）
    StateRecordingUser* MakeUser(const char* name, const char* id)
    {
        auto* u = new StateRecordingUser();
        u->set_name(name, id);
        u->set("level", (long)50);
        u->set("hp", (long)100);
        u->set("eff_hp", (long)100);
        u->move(room);
        u->delete_temp("client/loading");
        CMapping client;
        u->set_temp("client", client);
        return u;
    }

    // 是否收到"自己的"私有状态封包（C=23 T=<自身 object_id>）。
    // 房间广播的他人状态封包 T 是别人的 id，不会误判。
    static bool HasOwnState(StateRecordingUser* u)
    {
        char needle[64];
        snprintf(needle, sizeof(needle), "&C=23&T=%d&", u->object_id());
        return u->has(needle);
    }
};

// =====================================================================
// AP 闸门：战斗中行动点不足 → 拒绝施法，且不扣行动点
// =====================================================================
TEST_F(SkillCastTest, Cast_InFight_ApGateBlocksWhenInsufficient)
{
    caster->set_temp("fight/is_fighting", 1);
    caster->set_temp("fight/ap", (long)10);  // < 70

    EXPECT_EQ(0, skill->cast("strike", caster, target));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("AP不足"))
        << "行动点不足应被拦下";
    EXPECT_EQ(10, caster->query_temp("fight/ap")) << "被拒的施法不扣行动点";
}

// =====================================================================
// AP 扣除：施法成功后按耗费扣减
// =====================================================================
TEST_F(SkillCastTest, Cast_InFight_ApChargedAfterSuccess)
{
    caster->set_temp("fight/is_fighting", 1);
    caster->set_temp("fight/ap", (long)100);

    EXPECT_EQ(1, skill->cast("strike", caster, target));
    EXPECT_EQ(100 - EXPECTED_AP, caster->query_temp("fight/ap"))
        << "施法成功应扣掉法术的 AP 耗费";
}

// =====================================================================
// 非战斗：施法不涉及行动点
// =====================================================================
TEST_F(SkillCastTest, Cast_OutOfFight_NoApRequired)
{
    EXPECT_EQ(1, skill->cast("strike", caster, target)) << "非战斗不查行动点";
    EXPECT_EQ(0, caster->query_temp("fight/ap")) << "非战斗不动行动点";
}

// =====================================================================
// 受术者状态刷新：对敌法术（CAST_TO_OTHER）要通知目标
// =====================================================================
TEST_F(SkillCastTest, Cast_TargetStateRefreshed_ForOtherTarget)
{
    EXPECT_FALSE(HasOwnState(target)) << "施法前目标没有自己的状态封包";

    EXPECT_EQ(1, skill->cast("strike", caster, target));
    EXPECT_TRUE(HasOwnState(target)) << "受术者应收到状态刷新封包";
}

// =====================================================================
// 受术者状态刷新：非敌意法术同样刷新（法术可能改了目标的 hp/mp）
// =====================================================================
TEST_F(SkillCastTest, Cast_TargetStateRefreshed_ForNonHostileMagic)
{
    EXPECT_EQ(1, skill->cast("mend", caster, target));
    EXPECT_TRUE(HasOwnState(target)) << "受术者应收到状态刷新封包";
}

// =====================================================================
// 无目标法术（CAST_TO_NONE）：不刷新他人状态
// =====================================================================
TEST_F(SkillCastTest, Cast_NoTargetState_ForNoneTargetMagic)
{
    EXPECT_EQ(1, skill->cast("focus", caster, target));
    EXPECT_FALSE(HasOwnState(target)) << "CAST_TO_NONE 的法术不刷新受术者状态";
}

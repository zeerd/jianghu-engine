// test_cmd_wizard_dice.cpp - dice 命令（Lua 化）经真实命令分发链的行为
//
// dice 已从 C++ 巫师表移除（command/dice.lua），经真实分发链
// （CCmdHandle::handlecommand → Lua commands）验证权限门槛与回落行为。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include "Char.h"
#include "CmdHandle.h"
#include "CmdWizard.h"
#include "Container.h"
#include "User.h"
#include "test_lua_env.h"         // ensure_lua_env
#include "test_state_recorder.h"  // StateRecordingUser

namespace {

// 暴露 protected 的 handlecommand，走 C++ 三表 → Lua commands 的完整分发链。
class DiceHarness : public CCmdHandle {
public:
    int Dispatch(const char* cmd, CChar* me, int32_t target, const char* arg)
    {
        return handlecommand(const_cast<char*>(cmd), me, target,
                             const_cast<char*>(arg));
    }
};

}  // namespace

// dice 不在 C++ 巫师表：handlecommand 未命中返回 -1（交回分发链）
TEST(DiceCommand, NotInCppWizardTable)
{
    CContainer room;
    room.set_name("dice_room", "dice_room");
    CChar me;
    me.set("name", "测试巫师");
    me.set("level", (long)100);
    me.move(&room);

    CCmdWizard cmd;
    EXPECT_EQ(cmd.handlecommand((char*)"dice", &me, 0, (char*)"6"), -1);

    me.move(nullptr);
    room.move(nullptr);
}

class CmdWizardDiceTest : public ::testing::Test {
protected:
    CContainer room;
    StateRecordingUser* wiz;
    DiceHarness harness;

    void SetUp() override
    {
        ensure_lua_env();
        room.set_name("dice_room", "dice_room");
        wiz = new StateRecordingUser();
        wiz->set_name("测试巫师", "wizdice");
        wiz->set("grant", (long)200);  // wizardp 门槛
        wiz->move(&room);
        wiz->clear();  // 隔离 move 触发的投递
    }

    void TearDown() override
    {
        wiz->move(nullptr);
        room.move(nullptr);
        delete wiz;
    }
};

// 非巫师：脚本自检拒绝并返回 -1，命令让回分发链 → 0，无输出。
TEST_F(CmdWizardDiceTest, NonWizardFallsThrough)
{
    StateRecordingUser plain;
    plain.set_name("普通玩家", "pl1");
    plain.set("grant", (long)30);
    plain.move(&room);
    plain.clear();

    EXPECT_EQ(harness.Dispatch("dice", &plain, 0, "6"), 0);
    EXPECT_EQ(plain.msgs.size(), 0u);

    plain.move(nullptr);
}

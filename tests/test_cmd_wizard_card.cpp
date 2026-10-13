// test_cmd_wizard_card.cpp - Unit tests for CmdWizardCard.cpp
//
// createcard 走 C++ 巫师表；dice 已 Lua 化（command/dice.lua），
// 经真实命令分发链（CCmdHandle::handlecommand）验证行为不变。
#include "pch.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Char.h"
#include "CmdHandle.h"
#include "CmdWizard.h"
#include "Container.h"
#include "User.h"
#include "test_char_common.h"     // CharMockDb
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

// 从 "&C=<n>&M=<text>&" 中取出 <text>。
std::string Payload(const std::string& msg)
{
    size_t p = msg.find("&M=");
    if (p == std::string::npos) return "";
    size_t q = msg.rfind('&');
    if (q <= p + 3) return "";
    return msg.substr(p + 3, q - (p + 3));
}

bool IsAllDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (char c : s)
        if (!isdigit(static_cast<unsigned char>(c))) return false;
    return true;
}

}  // namespace

class CmdWizardCardTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallCreateCard(CChar* me, int32_t target, char* arg)
    {
        CCmdWizard cmd;
        return cmd.handlecommand((char*)"createcard", me, target, arg);
    }

    CChar* me;
    CContainer* room;

    void SetUp() override
    {
        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试巫师");
        me->set("id", "wiz1");
        me->set("level", (long)100);
        me->set("grant", (long)200);
        me->set("hp", (long)1000);
        me->move(room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }
};

TEST_F(CmdWizardCardTest, CreateCard_NoDb)
{
    // create_card 真的执行 INSERT（mock 记录 LastSql）→ 命令完成返回 1
    CharMockDb db;
    g_db       = &db;
    int result = CallCreateCard(me, 0, (char*)"");
    EXPECT_EQ(result, 1);
    EXPECT_FALSE(db.LastSql().empty());
    g_db = nullptr;
}

TEST_F(CmdWizardCardTest, Dice_NotInCppTable)
{
    // dice 已从 C++ 巫师表移除：handlecommand 未命中返回 -1（交回分发链）
    CCmdWizard cmd;
    EXPECT_EQ(cmd.handlecommand((char*)"dice", me, 0, (char*)"6"), -1);
}

// dice 命令不在 C++ 表；权限门槛与分发回落经真实分发链验证。
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
TEST_F(CmdWizardDiceTest, Dice_NonWizardFallsThrough)
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

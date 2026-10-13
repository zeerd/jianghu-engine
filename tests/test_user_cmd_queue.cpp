// test_user_cmd_queue.cpp - Unit tests for CUser command queue
// (unique_ptr<char[]> migration).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include <cstdio>
#include <cstring>
#include <string>
#include "User.h"
#include "Container.h"

// ====================================================================
// Test fixture: create a CUser with minimal setup
// ====================================================================
class UserCmdQueueTest : public ::testing::Test {
protected:
    CUser* user;

    void SetUp() override
    {
        user = new CUser();
        user->set("id", "test_queue");
        user->set("name", "测试玩家");
        user->set("gender", "男性");
    }

    void TearDown() override { delete user; }

    // Build a command string with this user's object_id as prefix
    // (space-separated)
    std::string MakeCmd(const char* command)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%ld %s", user->object_id(), command);
        return std::string(buf);
    }
};

// ====================================================================
// Basic push/pop: verify queue mechanics
// ====================================================================
TEST_F(UserCmdQueueTest, PushOneCommand)
{
    std::string cmd = MakeCmd("look");
    user->push_command(const_cast<char*>(cmd.c_str()), (int)cmd.size() + 1);
    // HandleCommand processes and returns 1 if a command was dequeued
    // (may return 0 if actor check fails or command execution fails, but
    // the command should still be consumed from the queue)
    user->handle_command();
    // Queue should be empty now — second call returns 0
    EXPECT_EQ(user->handle_command(), 0);
}

TEST_F(UserCmdQueueTest, EmptyQueueReturnsZero)
{
    EXPECT_EQ(user->handle_command(), 0);
}

TEST_F(UserCmdQueueTest, PushMultipleCommands)
{
    std::string cmd1 = MakeCmd("look");
    std::string cmd2 = MakeCmd("score");
    std::string cmd3 = MakeCmd("inventory");

    user->push_command(const_cast<char*>(cmd1.c_str()), (int)cmd1.size() + 1);
    user->push_command(const_cast<char*>(cmd2.c_str()), (int)cmd2.size() + 1);
    user->push_command(const_cast<char*>(cmd3.c_str()), (int)cmd3.size() + 1);

    // Each HandleCommand dequeues one command
    user->handle_command();
    user->handle_command();
    user->handle_command();
    // Queue should now be empty
    EXPECT_EQ(user->handle_command(), 0);
}

TEST_F(UserCmdQueueTest, SystemCommandExecutedImmediately)
{
    // "0 xxx" is a system command, executed immediately without queuing
    char cmd[] = "0 look";
    user->push_command(cmd, (int)strlen(cmd) + 1);
    // Queue should still be empty
    EXPECT_EQ(user->handle_command(), 0);
}

// ====================================================================
// Destruction safety: unique_ptr auto-releases
// ====================================================================
TEST_F(UserCmdQueueTest, DestructorReleasesCommands)
{
    // push 两条 → 全部消费 → 空队列返回 0 → 析构 unique_ptr 释放（不崩）
    // （合并原 DestructorNoLeak：push+handle+delete 契约重复）
    std::string cmd1 = MakeCmd("look");
    std::string cmd2 = MakeCmd("score");
    user->push_command(const_cast<char*>(cmd1.c_str()), (int)cmd1.size() + 1);
    user->push_command(const_cast<char*>(cmd2.c_str()), (int)cmd2.size() + 1);

    user->handle_command();  // 消费1（返回值依赖 actor 检查，不断言）
    user->handle_command();  // 消费2
    EXPECT_EQ(user->handle_command(), 0);  // 队列已空

    // unique_ptr 应在析构时自动释放，不崩不漏
    delete user;
    user = nullptr;
}

// test_cmd_unknown_log.cpp — CCmdHandle::command「未知的命令」分支的 WARN
// 落日志
//
// 契约：整条分发链（房间/物件截获 → 频道 → C++ 三表 → Lua 命令 →
// emote）都没接住 且没有 notify_fail
// 文案时，命令入口回玩家「未知的命令。」、返回 0，并把原始整行 （命令词 + 目标
// + 参数）以 WARN 级别写进日志，供排查拼写与参数问题。
//
// 现有覆盖为什么抓不住：test_cmd_gate.cpp 只断言返回值与 g_notify_fail_msg，
// 从不读日志落盘内容；这行日志被删掉、只记命令词不记参数、或级别被改成
// INFO/TRACE/ERROR 时，用例都不会变红。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <stdio.h>
#include <string>
#include <time.h>

#include "Char.h"
#include "Global.h"
#include "Logger.h"
#include "Room.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];

namespace {

class CommandUnknownLogTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ensure_lua_env();
        g_notify_fail_msg[0] = '\0';
        m_orig_level         = logger_get_level();

        room = new CRoom();
        room->set_name("无人角落", "unk_room");

        me = new CChar();
        me->set("name", "迷路的人");
        me->set("id", "unk_me");
        me->set("hp", (long)100);
        me->move(room);
    }

    void TearDown() override
    {
        logger_set_level(m_orig_level);
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }

    // 发一条整链都没接住的命令（首词非数字 → 不带目标，不走物件截获）
    int SendUnknown(const char* line)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s", line);
        return me->command(buf);
    }

    // 本次运行的日志文件：与 Logger 落盘同一路径（M-D[-runid].txt）
    static std::string LogFile()
    {
        time_t t       = time(NULL);
        struct tm* now = localtime(&t);
        return logger_log_file_name(now->tm_mon + 1, now->tm_mday);
    }

    static std::string ReadAll(const std::string& file)
    {
        FILE* fp = fopen(file.c_str(), "rb");
        if (!fp) return "";
        std::string content;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) content.append(buf, n);
        fclose(fp);
        return content;
    }

    CRoom* room           = nullptr;
    CChar* me             = nullptr;
    LogLevel m_orig_level = LOG_LVL_TRACE;
};

}  // namespace

// 未知命令：返回 0、无 notify_fail 文案，且原始整行（含参数）以 WARN 落日志
TEST_F(CommandUnknownLogTest, UnknownCommand_WarnLogsFullLine)
{
    logger_set_level(LOG_LVL_WARN);

    EXPECT_EQ(0, SendUnknown("zzz_unk_cmd foo bar"))
        << "分发链未走到底，未知命令分支没被执行";
    EXPECT_STREQ(g_notify_fail_msg, "")
        << "有 notify_fail 文案就落不到未知命令分支";

    const std::string content = ReadAll(LogFile());
    EXPECT_NE(content.find("raw=[zzz_unk_cmd foo bar]"), std::string::npos)
        << "原始整行未按 WARN 落日志: " << LogFile();
    EXPECT_NE(content.find("target=[foo] arg=[bar]"), std::string::npos)
        << "命令词/目标/参数拆分结果未一并记录";
}

// 级别高于 WARN（ERROR/FATAL）时不再落这行，级别契约的反向对照
TEST_F(CommandUnknownLogTest, UnknownCommand_NotLoggedAboveWarnLevel)
{
    logger_set_level(LOG_LVL_ERROR);

    EXPECT_EQ(0, SendUnknown("zzz_unk_2nd foo bar"))
        << "分发链未走到底，未知命令分支没被执行";

    const std::string content = ReadAll(LogFile());
    EXPECT_EQ(content.find("raw=[zzz_unk_2nd foo bar]"), std::string::npos)
        << "这条日志的实际级别高于 WARN: " << LogFile();
}

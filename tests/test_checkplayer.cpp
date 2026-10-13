// test_checkplayer.cpp — Unit tests for CheckPlayer (LuaLogin)
//
// Tests the CheckPlayer function which calls login.on_login in Lua.
// Verifies attribute validation: total == 84 (pass) vs total > 84 (fail).
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Container.h"
#include "LuaLogin.h"
#include "IniParser.h"  // ini_get(script_root)
#include <filesystem>
#include <fstream>
#include "test_lua_env.h"

// ====================================================================
// Test fixture: initializes Lua engine from test_config.ini
// ====================================================================
namespace fs = std::filesystem;
// 换装 login.lua：写入 → check_player 经 load_script 按 hash 自动重载读到
// 新文件 → 收尾删除（下次 load_script 自动回落到默认根的样例脚本）
class ScopedLoginScript {
public:
    explicit ScopedLoginScript(const char* content)
    {
        namespace fs           = std::filesystem;
        const std::string root = ini_get("lua", "script_root", "");
        m_file                 = fs::path(root) / "data" / "login.lua";
        fs::create_directories(m_file.parent_path());
        m_had_orig = fs::exists(m_file);
        if (m_had_orig)
            fs::copy_file(m_file, fs::path(m_file.string() + ".bak"),
                          fs::copy_options::overwrite_existing);
        std::ofstream out(m_file);
        out << content;
    }

    ~ScopedLoginScript()
    {
        std::error_code ec;
        if (m_had_orig) {
            fs::rename(fs::path(m_file.string() + ".bak"), m_file, ec);
        }
        else {
            fs::remove(m_file, ec);
        }
    }

    ScopedLoginScript(const ScopedLoginScript&)            = delete;
    ScopedLoginScript& operator=(const ScopedLoginScript&) = delete;

private:
    std::filesystem::path m_file;
    bool m_had_orig = false;
};
class CheckPlayerTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }

    void TearDown() override
    {
        // Don't shutdown — the singleton persists across tests
    }

    // Helper: create a CChar with specified 4 attributes
    CChar* MakeChar(int str, int con, int intel, int dex)
    {
        CChar* ch = new CChar();
        ch->set("str", (long)str);
        ch->set("con", (long)con);
        ch->set("int", (long)intel);
        ch->set("dex", (long)dex);
        ch->set("reward/attrib", (long)0);
        return ch;
    }
};

// ====================================================================
// Test: 4 attributes sum to exactly 84 → should PASS (return 0)
// ====================================================================
// ====================================================================

TEST_F(CheckPlayerTest, LoginScript_MissingOnLogin_FailsGracefully)
{
    ScopedLoginScript guard("return { }");
    CChar* ch = MakeChar(20, 21, 22, 21);
    EXPECT_EQ(-1, check_player(ch, 1)) << "无 on_login → -1(24-25)";
    delete ch;
}

TEST_F(CheckPlayerTest, LoginScript_OnLoginRuntimeError_FailsGracefully)
{
    ScopedLoginScript guard(
        "return { on_login = function(me, newenter) error('boom') end }");
    CChar* ch = MakeChar(20, 21, 22, 21);
    EXPECT_EQ(-1, check_player(ch, 1)) << "钩子运行期报错 → -1(33-35)";
    delete ch;
}
TEST_F(CheckPlayerTest, AttributesEqual84_Pass)
{
    // 20+21+22+21 = 84
    CChar* ch  = MakeChar(20, 21, 22, 21);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0) << "Attributes sum to 84, should pass";
    delete ch;
}

// ====================================================================
// Test: 4 attributes sum to less than 84 → should PASS (return 0)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesLessThan84_Pass)
{
    // 10+10+10+10 = 40
    CChar* ch  = MakeChar(10, 10, 10, 10);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0) << "Attributes sum to 40 (< 84), should pass";
    delete ch;
}

// ====================================================================
// Test: 4 attributes sum to 85 (> 84) → should FAIL (return 1)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesExceed84_Fail)
{
    // 21+21+22+21 = 85
    CChar* ch  = MakeChar(21, 21, 22, 21);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1) << "Attributes sum to 85 (> 84), should fail";
    delete ch;
}

// ====================================================================
// Test: 4 attributes sum to 100 (way over) → should FAIL (return 1)
// ====================================================================
TEST_F(CheckPlayerTest, AttributesWayOver_Fail)
{
    // 25+25+25+25 = 100
    CChar* ch  = MakeChar(25, 25, 25, 25);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1) << "Attributes sum to 100, should fail";
    delete ch;
}

// ====================================================================
// Test: with reward/attrib bonus, higher total is allowed
// ====================================================================
TEST_F(CheckPlayerTest, WithBonus_Pass)
{
    // 25+25+25+25 = 100, but reward/attrib = 20 → limit = 104
    CChar* ch = MakeChar(25, 25, 25, 25);
    ch->set("reward/attrib", (long)20);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 0)
        << "Attributes sum to 100 with bonus 20 (limit=104), should pass";
    delete ch;
}

// ====================================================================
// Test: with reward/attrib bonus, still fails if over limit
// ====================================================================
TEST_F(CheckPlayerTest, WithBonus_StillFail)
{
    // 25+25+25+25 = 100, reward/attrib = 10 → limit = 94
    CChar* ch = MakeChar(25, 25, 25, 25);
    ch->set("reward/attrib", (long)10);
    int result = check_player(ch, 1);
    EXPECT_EQ(result, 1)
        << "Attributes sum to 100 with bonus 10 (limit=94), should fail";
    delete ch;
}

// test_dispatch_encoding.cpp — 房间指令分发的编码契约
//
// 构建与运行（唯一认可方式）: .\run-coverage.ps1（仓库根）
//
// 契约：脚本源码加载期 UTF-8→GBK（load_file_as_gbk），Lua 字符串空间是
// GBK；房间分发（CLuaRoomT::handle_action / valid_leave）投递原始 GBK
// 入参，与 NPC/Item 分发（LuaNpc.inl / LuaItem.inl）口径一致，脚本中文字
// 面量可直接比较。历史上房间分发单方面转 UTF-8，与字面量反向——靠 ASCII
// 比较与脚本侧 utf8_to_gbk 回转掩盖（两条补偿链已随契约统一移除）。
//
// 夹具 t_encoding_room 的字面量（"测试"/"改名"/"出口"）加载期已是 GBK；
// 入参用显式 GBK 字节转义，避免测试源码字符集差异影响断言。
#include "pch.h"

#include "Char.h"
#include "Room.h"
#include "RoomMgr.h"
#include "test_lua_env.h"

namespace {
// GBK 字节：测试=B2 E2 CA D4，改名=B8 C4 C3 FB，出口=B3 F6 BF DA
const char kGbkCeshi[]  = "\xB2\xE2\xCA\xD4";
const char kGbkGaiming[] = "\xB8\xC4\xC3\xFB";
const char kGbkChukou[]  = "\xB3\xF6\xBF\xDA";

class DispatchEncodingTest : public ::testing::Test {
protected:
    CRoom* room = nullptr;
    CChar* me   = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        me   = new CChar();
        me->set("id", "tester");
        room = load_room("t_encoding_room");
        ASSERT_NE(room, nullptr) << "夹具脚本缺失或加载失败";
    }

    void TearDown() override
    {
        me->move(nullptr);
        delete me;
    }
};
}  // namespace

TEST_F(DispatchEncodingTest, HandleActionArgMatchesGbkLiteral)
{
    EXPECT_EQ(
        room->handle_action((char*)"run", me, (char*)kGbkCeshi, (char*)""),
        11)
        << "GBK arg 应与脚本字面量直接相等（分发不做转码）";
}

TEST_F(DispatchEncodingTest, HandleActionCommMatchesGbkLiteral)
{
    EXPECT_EQ(
        room->handle_action((char*)kGbkGaiming, me, (char*)"", (char*)""), 12)
        << "GBK comm 应与脚本字面量直接相等";
}

TEST_F(DispatchEncodingTest, HandleActionUnknownReturnsZero)
{
    EXPECT_EQ(room->handle_action((char*)"unknown", me, (char*)"", (char*)""),
              0);
}

TEST_F(DispatchEncodingTest, ValidLeaveDirMatchesGbkLiteral)
{
    EXPECT_EQ(room->valid_leave(me, kGbkChukou), 13)
        << "GBK dir（出口）应与脚本字面量直接相等";
    EXPECT_EQ(room->valid_leave(me, "somewhere"), 1);
}

TEST_F(DispatchEncodingTest, Utf8ToGbkBindingStaysAvailable)
{
    // 分发契约统一后脚本侧已无调用方，但 utf8_to_gbk 仍是内容作者的
    // 公开绑定（构造 UTF-8 文本时使用），保持可用
    lua_State* L = ensure_lua_env();
    ASSERT_NE(L, nullptr);
    ASSERT_EQ(luaL_dostring(L, "assert(utf8_to_gbk('abc') == 'abc')"), LUA_OK)
        << lua_tostring(L, -1);
}

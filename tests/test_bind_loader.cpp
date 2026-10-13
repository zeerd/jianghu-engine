// test_bind_loader.cpp — binds that need a live CLuaEngine (room/skill/script

// loaders + g_LuaEngine-backed queries). Uses the engine-initialized fixture.

#include "test_bind_common.h"

#include "Item.h"

#include <cstdio>

#include <cstdlib>

// LuaEngine.cpp 自由函数：文件读取转 GBK（空文件支在 93-95）

extern std::string load_file_as_gbk(
    const char* path);  // CItem 非角色参数（l_get_my_house 的 !ch 支）

class BindLoaderTest : public BindEngineTest {
protected:
    CChar ch;

    void SetUp() override

    {
        BindEngineTest::SetUp();

        ch.set_name("ldr", "ldr1");

        ch.set("id", "ldr1");
    }
};

// ---- RoomMgr.cpp ----

TEST_F(BindLoaderTest, RoomCountAndName)

{
    int k = call(l_room_count, 0);

    EXPECT_EQ(1, k);

    EXPECT_GE(lua_tointeger(L, -1), 0);

    lua_pop(L, k);

    lua_pushinteger(L, 0);

    k = call(l_global_room_name, 1);

    EXPECT_EQ(1, k);

    lua_pop(L, k);
}

TEST_F(BindLoaderTest, LoadRoom)

{
    lua_pushstring(L, "nonexistent_room_zzz");

    int k = call(l_load_room, 1);

    EXPECT_EQ(1, k);

    lua_pop(L, k);
}

// ---- SkillMgr.cpp ----

TEST_F(BindLoaderTest, LoadSkill)

{
    lua_pushstring(L, "nonexistent_skill_zzz");

    int k = call(l_load_skill, 1);

    EXPECT_EQ(1, k);

    lua_pop(L, k);
}

// ---- LuaEngine.cpp ----

TEST_F(BindLoaderTest, script_exists)

{
    lua_pushstring(L, "no_such_script_zzz");

    int k = call(l_script_exists, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isboolean(L, -1));

    lua_pop(L, k);
}

// ---- Resident.cpp: get_my_house (no resident -> nil, no load_room call) ----

TEST_F(BindLoaderTest, GetMyHouseNone)

{
    lua_bind_push_object(L, &ch);

    int k = call(l_get_my_house, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);
}

// ---- Resident.cpp: get_my_house 非角色参数 → nil(20-24) ----

TEST_F(BindLoaderTest, GetMyHouseNonCharArg)

{
    CItem thing;

    thing.set_name("物件", "res_thing");

    lua_bind_push_object(L, &thing);

    int k = call(l_get_my_house, 1);

    EXPECT_EQ(1, k) << "返回一个值";

    EXPECT_TRUE(lua_isnil(L, -1)) << "非角色对象 → nil(21)";

    lua_pop(L, k);
}

// ---- LuaEngine.cpp: load_file_as_gbk 空文件 → fclose+空串(93-95) ----
TEST_F(BindLoaderTest, LoadFileAsGbkEmptyFileReturnsEmpty)
{
    const char* tmp = getenv("TEMP");
    ASSERT_NE(tmp, nullptr);
    std::string path = std::string(tmp) + "\\lua_engine_empty_probe.tmp";

    FILE* f = fopen(path.c_str(), "wb");
    ASSERT_NE(f, nullptr);
    fclose(f);  // 0 字节文件 → size<=0 支

    std::string content = load_file_as_gbk(path.c_str());
    EXPECT_TRUE(content.empty()) << "空文件 → 空串并 fclose(93-95)";

    remove(path.c_str());
}

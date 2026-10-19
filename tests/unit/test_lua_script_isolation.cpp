// test_lua_script_isolation.cpp — 同名脚本按类型隔离的单元测试
//
// 验证 m_scriptMap 使用复合 key（type_dir/name）后，
// 不同类型的同名脚本（如 item/hammer 与 skill/hammer）各自独立。
//
// 修复前：m_scriptMap["hammer"] 只能指向一个文件，后扫描的覆盖先扫描的。
// 修复后：m_scriptMap["item/hammer"] 和 m_scriptMap["skill/hammer"] 独立存在。
//
#include "pch.h"
#include "lua/LuaEngine.h"
#include <string>

// friend struct：访问 CLuaEngine 的 private 成员
struct CLuaScriptIsolationTest {
    static std::map<std::string, std::string>& ScriptMap(CLuaEngine* e)
    {
        return e->m_script_map;
    }
    static std::string get_script_path(CLuaEngine* e, const char* name,
                                       LuaScriptType type)
    {
        return e->get_script_path(name, type);
    }
    static std::string get_script_dir(CLuaEngine* e, LuaScriptType type)
    {
        return e->get_script_dir(type);
    }
};

// ====================================================================
// 复合 key 格式验证
// ====================================================================
TEST(ScriptIsolationTest, CompositeKeyFormat)
{
    CLuaEngine* engine = CLuaEngine::instance();
    ASSERT_NE(engine, nullptr);

    // 验证 GetScriptDir 返回正确的子目录名
    EXPECT_EQ(CLuaScriptIsolationTest::get_script_dir(engine, LUA_SCRIPT_NPC),
              "npc");
    EXPECT_EQ(CLuaScriptIsolationTest::get_script_dir(engine, LUA_SCRIPT_ITEM),
              "item");
    EXPECT_EQ(CLuaScriptIsolationTest::get_script_dir(engine, LUA_SCRIPT_SKILL),
              "skill");
    EXPECT_EQ(CLuaScriptIsolationTest::get_script_dir(engine, LUA_SCRIPT_ROOM),
              "room");
}

// ====================================================================
// m_scriptMap 复合 key 隔离验证
// ====================================================================
TEST(ScriptIsolationTest, SameNameDifferentTypeIndependent)
{
    CLuaEngine* engine = CLuaEngine::instance();
    ASSERT_NE(engine, nullptr);

    auto& scriptMap = CLuaScriptIsolationTest::ScriptMap(engine);

    // 模拟扫描：为同名脚本 "test_dup" 注册不同类型的条目
    std::string itemKey  = "item/test_dup";
    std::string skillKey = "skill/test_dup";
    std::string npcKey   = "npc/test_dup";

    scriptMap[itemKey]  = "/fake/path/item/test_dup.lua";
    scriptMap[skillKey] = "/fake/path/skill/test_dup.lua";
    scriptMap[npcKey]   = "/fake/path/npc/test_dup.lua";

    // 验证三个条目各自独立存在
    EXPECT_EQ(scriptMap.count(itemKey), (size_t)1);
    EXPECT_EQ(scriptMap.count(skillKey), (size_t)1);
    EXPECT_EQ(scriptMap.count(npcKey), (size_t)1);

    // 验证路径各不相同
    EXPECT_NE(scriptMap[itemKey], scriptMap[skillKey]);
    EXPECT_NE(scriptMap[itemKey], scriptMap[npcKey]);
    EXPECT_NE(scriptMap[skillKey], scriptMap[npcKey]);

    // 验证 GetScriptPath 对不同类型返回不同路径
    std::string itemPath = CLuaScriptIsolationTest::get_script_path(
        engine, "test_dup", LUA_SCRIPT_ITEM);
    std::string skillPath = CLuaScriptIsolationTest::get_script_path(
        engine, "test_dup", LUA_SCRIPT_SKILL);
    std::string npcPath = CLuaScriptIsolationTest::get_script_path(
        engine, "test_dup", LUA_SCRIPT_NPC);

    // 因为 m_scriptMap 中有对应条目，GetScriptPath 应返回注册的路径
    EXPECT_EQ(itemPath, "/fake/path/item/test_dup.lua");
    EXPECT_EQ(skillPath, "/fake/path/skill/test_dup.lua");
    EXPECT_EQ(npcPath, "/fake/path/npc/test_dup.lua");

    // 清理
    scriptMap.erase(itemKey);
    scriptMap.erase(skillKey);
    scriptMap.erase(npcKey);
}

// ====================================================================
// GetScriptPath 回退路径验证
// ====================================================================
TEST(ScriptIsolationTest, FallbackPathUsesType)
{
    CLuaEngine* engine = CLuaEngine::instance();
    ASSERT_NE(engine, nullptr);

    // 对于不在 m_scriptMap 中的名字，GetScriptPath 应回退到
    // scriptRoot/type_dir/name.lua 格式
    std::string itemPath = CLuaScriptIsolationTest::get_script_path(
        engine, "nonexistent", LUA_SCRIPT_ITEM);
    std::string skillPath = CLuaScriptIsolationTest::get_script_path(
        engine, "nonexistent", LUA_SCRIPT_SKILL);

    // 回退路径应包含类型子目录
    EXPECT_NE(itemPath.find("/item/nonexistent.lua"), std::string::npos);
    EXPECT_NE(skillPath.find("/skill/nonexistent.lua"), std::string::npos);

    // 两条路径不应相同
    EXPECT_NE(itemPath, skillPath);
}

// ====================================================================
// 复合 key 覆盖行为验证（同类型同名才覆盖）
// ====================================================================
TEST(ScriptIsolationTest, OverrideOnlyWithinSameType)
{
    CLuaEngine* engine = CLuaEngine::instance();
    ASSERT_NE(engine, nullptr);

    auto& scriptMap = CLuaScriptIsolationTest::ScriptMap(engine);

    std::string key1 = "item/test_override";
    std::string key2 = "skill/test_override";

    // 先注册 item 条目
    scriptMap[key1] = "/first/path/item/test_override.lua";
    // 再注册 skill 条目（不同 key，不应覆盖 item）
    scriptMap[key2] = "/second/path/skill/test_override.lua";

    // item 条目应保持不变
    EXPECT_EQ(scriptMap[key1], "/first/path/item/test_override.lua");
    EXPECT_EQ(scriptMap[key2], "/second/path/skill/test_override.lua");

    // 同类型同名覆盖：更新 item 条目
    scriptMap[key1] = "/updated/path/item/test_override.lua";
    EXPECT_EQ(scriptMap[key1], "/updated/path/item/test_override.lua");
    // skill 条目不受影响
    EXPECT_EQ(scriptMap[key2], "/second/path/skill/test_override.lua");

    // 清理
    scriptMap.erase(key1);
    scriptMap.erase(key2);
}

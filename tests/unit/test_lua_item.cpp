// test_lua_item.cpp - CLuaItemT<T> 的脚本钩子面（src/lua/LuaItem.inl）
//
// 覆盖目标：
//   - 各成员的脚本结果回读（do_use/do_look/Can_Use/Can_Equip/
//     query_autoload/handle_action×2/do_action）
//   - can_combine_in_box 的脚本分支
//   - reload_script 的缓存 ref 支
//   - create(flag) 的缺脚本/空脚本双兜底
//   - lua_do_action_base 的环境推导
//
//
#include "pch.h"

#include "Char.h"
#include "Container.h"
#include <windows.h>  // LuaItem.inl 需 UUID（rpc 链经 windows.h）
#include "LuaItem.h"
#include "LuaBind.h"              // lua_bind_push_object
#include "LuaBindDecls.h"         // l_script_table
#include "LuaInstanceRegistry.h"  // lua_item_notify_reload
#include "test_lua_env.h"

class LuaItemTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_NE(ensure_lua_env(), nullptr); }
};

// 全钩子脚本：一次装载，逐成员回读整数结果
TEST_F(LuaItemTest, HookResultsReadBackAcrossMembers)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("测试道具", "li_full");
    item.create(0);  // 载 m_script_ref + 跑 create 钩子
    EXPECT_EQ(1, item.query("li_created")) << "create 钩子执行(脚本)";

    CChar me;
    me.set_name("使用者", "li_user");
    CContainer target;
    target.set_name("目标", "li_target");

    EXPECT_EQ(1, item.do_use(&me, &target)) << "do_use 回读(142-144)";
    EXPECT_EQ(1, item.do_look(&me)) << "do_look 回读(174-175)";
    EXPECT_EQ(1, item.can_use()) << "Can_Use 回读(204-205)";
    EXPECT_EQ(1, item.can_equip(&me)) << "Can_Equip 回读(236-237)";
    EXPECT_EQ(1, item.query_autoload()) << "query_autoload 回读(298-299)";
    EXPECT_EQ(1, item.handle_action((char*)"act", &me, (char*)""))
        << "handle_action3 回读(331-332)";
    EXPECT_EQ(1, item.handle_action((char*)"act", &me, (char*)"", (char*)""))
        << "handle_action4 回读(369-370)";
    EXPECT_EQ(1, item.do_action(0, &me)) << "do_action 回读(438-439)";
}

// ---- can_use 的可用性判定 ----
// 声明了 Can_Use 就按它回读；没声明时按「是否实现 do_use」判定——
// 实现了使用行为的物品即可使用，两者都没有才落回 C++ 兜底。
TEST_F(LuaItemTest, CanUseFallsBackToDoUsePresence)
{
    CLuaItem item("fixture_usable_item");
    item.set_name("只有使用行为", "li_usable");
    item.create(0);
    ASSERT_EQ(1, item.query("li_created")) << "create 钩子已执行";

    lua_State* L        = ensure_lua_env();
    const int stack_top = lua_gettop(L);

    EXPECT_EQ(1, item.can_use()) << "缺 Can_Use 但有 do_use → 可用";
    EXPECT_EQ(stack_top, lua_gettop(L)) << "两次取字段的收尾不能漏弹栈";
}

TEST_F(LuaItemTest, CanUseExplicitZeroOverridesDoUse)
{
    CLuaItem item("fixture_unusable_item");
    item.set_name("显式禁用", "li_unusable");
    item.create(0);
    ASSERT_EQ(1, item.query("li_created")) << "create 钩子已执行";

    EXPECT_EQ(0, item.can_use()) << "显式 Can_Use=0 压过 do_use 判定";
}

TEST_F(LuaItemTest, CanUseFallsBackToCppWithoutUseBehavior)
{
    CLuaItem blank("fixture_blank_item");
    blank.set_name("空钩子", "li_canuse_blank");
    blank.create(0);

    EXPECT_EQ(0, blank.can_use()) << "既无 Can_Use 也无 do_use → C++ 兜底";
}

TEST_F(LuaItemTest, CanCombineInBoxScriptBranch)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("测试道具", "li_comb");
    item.create(0);

    EXPECT_TRUE(item.can_combine_in_box())
        << "脚本返回 true(574-586)；缺省走 C++ 兜底(588)";
}

TEST_F(LuaItemTest, ReloadScriptUsesCachedRefAndRerunsCreate)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("测试道具", "li_reload");
    item.create(0);  // 先把脚本登进引擎缓存
    ASSERT_EQ(1, item.query("li_created"));

    item.del("li_created");
    item.reload_script();  // get_script 命中缓存 → ref → create()

    EXPECT_EQ(1, item.query("li_created"))
        << "reload 走缓存支并重跑 create(41-47)";
}

TEST_F(LuaItemTest, CreateFlagMissingScriptFallsBack)
{
    CLuaItem item("no_such_item_script_xyz");
    item.set_name("缺脚本", "li_missing");
    item.create(0);
    EXPECT_EQ(LUA_NOREF, item.get_script_ref())
        << "缺脚本保持 NOREF → C++ 兜底 T::create(flag)(61-62)";
}

TEST_F(LuaItemTest, CreateFlagBlankScriptFallsBackToCpp)
{
    CLuaItem item("fixture_blank_item");
    item.set_name("空钩子", "li_blank");
    item.create(0);  // 脚本无 create 钩子 → 71-73 兜底
    EXPECT_EQ(0, item.query("li_created")) << "空脚本无副作用(71-73)";
}

TEST_F(LuaItemTest, DoActionBaseDerivesMeFromEnvironment)
{
    CLuaItem item("fixture_blank_item");
    item.set_name("环境推导", "li_env");
    item.create(0);

    CChar carrier;
    carrier.set_name("携带者", "li_carrier");
    item.move(&carrier);

    EXPECT_EQ(0, item.lua_do_action_base(0, nullptr))
        << "me=null 时从环境角色推导(524-528)";
}
// ---- 0 返回值 pcall 的栈平衡 ----
// pcall 吃掉「函数 + 实参」后，栈里只剩脚本表；成功路径多弹一个就会把
// 栈底之下的位置收掉（栈下溢），随后任何 Lua API 调用都可能直接崩。
// 这里以 lua_gettop 前后相等钉住两个钩子的清理语义。
TEST_F(LuaItemTest, HookPcallKeepsStackBalanced)
{
    CLuaItem item("fixture_hook_item");
    item.set_name("钩子物品", "li_hook");
    item.create(0);
    ASSERT_EQ(1, item.query("li_created")) << "create 钩子已执行";

    lua_State* L        = ensure_lua_env();
    const int stack_top = lua_gettop(L);

    item.on_moved(nullptr);  // 走 0 结果 pcall 的成功路径
    EXPECT_EQ(stack_top, lua_gettop(L)) << "on_moved 收尾多弹了栈";

    CChar killer;
    killer.set_name("击杀者", "li_killer");
    item.owner_is_killed(&killer);  // 同一口径
    EXPECT_EQ(stack_top, lua_gettop(L)) << "owner_is_killed 收尾多弹了栈";

    EXPECT_EQ(1, item.query("moved_count")) << "on_moved 钩子执行";
    EXPECT_EQ(1, item.query("killed")) << "owner_is_killed 钩子执行";
}
// ---- instance reload bridge / query_attr 脚本支 / blank 逐成员兜底 ----

TEST_F(LuaItemTest, NotifyReloadRunsInstanceBridge)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("桥触发", "li_bridge");
    item.create(0);
    ASSERT_EQ(1, item.query("li_created"));

    item.del("li_created");
    lua_item_notify_reload("fixture_lua_item");  // registry → bridge → reload

    EXPECT_EQ(1, item.query("li_created"))
        << "bridge(17-19) → reload_script → create 重跑";
}

TEST_F(LuaItemTest, QueryAttrScriptBranches)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("属性脚本", "li_attr");
    item.create(0);

    EXPECT_EQ(0, item.query_attr(CMainObject::IS_FOOD))
        << "type≠food → pop 支(549-550)";
    EXPECT_EQ(1, item.query_attr(CMainObject::CAN_COMBINE))
        << "can_combine 字段回读(562-563)";
}

TEST_F(LuaItemTest, BlankScriptCppFallbackPerMember)
{
    CLuaItem blank("fixture_blank_item");
    blank.set_name("空钩子", "li_blank2");
    blank.create(0);

    CChar me;
    me.set_name("测试者", "li_fb_user");
    CChar killer;
    killer.set_name("击杀者", "li_fb_killer");

    EXPECT_EQ(0,
              blank.handle_action((char*)"act", &me, (char*)"", (char*)"tar"))
        << "handle_action4 无钩子 → C++ 兜底(341)";
    blank.owner_is_killed(&killer);  // 无钩子 → call_base(379)/pop(404)
    blank.on_moved(&me);             // 无钩子 → pop(474)

    blank.move(&me);  // 环境是角色 → do_action 可推导 me
    EXPECT_EQ(0, blank.do_action(0, nullptr))
        << "me=null 从环境推导 + C++ 兜底(415)";
}

// ---- script_table：对象回到自己的脚本表 ----
// self 是引擎压入的 userdata，脚本表字段在它上面取不到；基类模块要按名派发
// 子类覆盖项（food_base 的 finish_eat 钩子），只能由引擎按 registry
// 引用还原表。

TEST_F(LuaItemTest, ScriptTableReturnsOwnScriptTable)
{
    CLuaItem item("fixture_lua_item");
    item.set_name("脚本表", "li_script_table");
    item.create(0);
    ASSERT_NE(item.get_script_ref(), LUA_NOREF);

    lua_State* L = ensure_lua_env();
    lua_bind_push_object(L, &item);
    lua_pushcfunction(L, l_script_table);
    lua_insert(L, -2);  // script_table(self)
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        ADD_FAILURE() << "script_table 抛错: " << lua_tostring(L, -1);
        lua_pop(L, 1);
        return;
    }

    ASSERT_TRUE(lua_istable(L, -1)) << "有脚本 → 返回该脚本自己的表";
    lua_getfield(L, -1, "create");
    EXPECT_TRUE(lua_isfunction(L, -1)) << "表里是该脚本的字段";
    lua_pop(L, 2);
}

TEST_F(LuaItemTest, ScriptTableNilWithoutScript)
{
    CChar carrier;  // 非脚本对象：registry 里没有它的表
    lua_State* L = ensure_lua_env();
    lua_bind_push_object(L, &carrier);
    lua_pushcfunction(L, l_script_table);
    lua_insert(L, -2);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        ADD_FAILURE() << "script_table 抛错: " << lua_tostring(L, -1);
        lua_pop(L, 1);
        return;
    }

    EXPECT_TRUE(lua_isnil(L, -1)) << "无脚本 → nil，调用方据此回退默认实现";
    lua_pop(L, 1);
}

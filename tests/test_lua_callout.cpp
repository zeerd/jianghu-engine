// test_lua_callout.cpp — Lua call_out 注册表契约
//
// 经 l_call_out / l_remove_call_out 真实绑定边界驱动 CLuaEngine：
// 1. remove_call_out 只删发起对象自己的挂起回调，跨对象同名函数互不影响；
// 2. 回调执行期间 remove_call_out 删自己会被跳过，执行完由 execute_callout
//    按 key 清理。
#include "pch.h"

#include "Container.h"
#include "LuaBind.h"
#include "LuaBindDecls.h"
#include "LuaEngine.h"
#include "test_lua_env.h"

namespace {

// call_out 会把对象登记进全局心跳链表，而容器析构不注销；
// 测试对象离开作用域前必须撤登记，否则全局链表残留悬垂指针。
class HeartBeatGuard {
public:
    explicit HeartBeatGuard(CContainer* ob) : m_ob(ob) {}
    ~HeartBeatGuard() { m_ob->set_heart_beat(0); }

    HeartBeatGuard(const HeartBeatGuard&)            = delete;
    HeartBeatGuard& operator=(const HeartBeatGuard&) = delete;

private:
    CContainer* m_ob;
};

}  // namespace

class LuaCalloutTest : public ::testing::Test {
protected:
    lua_State* L           = nullptr;
    int m_script_ref       = LUA_NOREF;
    int m_saved_active_ref = LUA_NOREF;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        CLuaEngine* engine = CLuaEngine::instance();
        L                  = engine->get_state();
        m_saved_active_ref = engine->get_active_script_ref();
    }

    void TearDown() override
    {
        CLuaEngine* engine = CLuaEngine::instance();
        engine->set_active_script_ref(m_saved_active_ref);
        if (m_script_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, m_script_ref);
            m_script_ref = LUA_NOREF;
        }
        lua_pushnil(L);
        lua_setglobal(L, "callout_test_script");
        lua_pushnil(L);
        lua_setglobal(L, "callout_test_ran");
        lua_pushnil(L);
        lua_setglobal(L, "callout_test_removed");
        lua_pushnil(L);
        lua_setglobal(L, "callout_test_fn");
    }

    // 执行一段 Lua 源码建脚本表（源码须把表赋给全局 callout_test_script），
    // 并置为 active script ref：无脚本的普通对象经回退路径注册回调。
    bool LoadScriptTable(const char* src)
    {
        if (luaL_dostring(L, src) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            ADD_FAILURE() << "lua error: " << (err ? err : "?");
            lua_pop(L, 1);
            return false;
        }
        lua_getglobal(L, "callout_test_script");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            ADD_FAILURE() << "callout_test_script is not a table";
            return false;
        }
        m_script_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        CLuaEngine::instance()->set_active_script_ref(m_script_ref);
        return true;
    }

    // self:call_out(func, delay) -> 回调 ID（<=0 表示注册失败）
    int CallOut(CContainer* ob, const char* func_name, int delay)
    {
        lua_bind_push_object(L, ob);
        lua_pushstring(L, func_name);
        lua_pushinteger(L, delay);
        lua_pushcfunction(L, l_call_out);
        lua_insert(L, -4);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
            lua_pop(L, 1);
            return -1;
        }
        int id = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return id;
    }

    // self:remove_call_out(func) -> 移除数量
    int RemoveCallOut(CContainer* ob, const char* func_name)
    {
        lua_bind_push_object(L, ob);
        lua_pushstring(L, func_name);
        lua_pushcfunction(L, l_remove_call_out);
        lua_insert(L, -3);
        if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
            lua_pop(L, 1);
            return -1;
        }
        int removed = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return removed;
    }

    // self:call_out(fn_global, delay) -> 回调 ID（fn_global 是全局函数值）
    int CallOutFunc(CContainer* ob, const char* fn_global, int delay)
    {
        lua_bind_push_object(L, ob);
        lua_getglobal(L, fn_global);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            ADD_FAILURE() << fn_global << " is not a function";
            return -1;
        }
        lua_pushinteger(L, delay);
        lua_pushcfunction(L, l_call_out);
        lua_insert(L, -4);
        if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
            lua_pop(L, 1);
            return -1;
        }
        int id = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return id;
    }

    // 读取回调写入的 Lua 全局量（未写入时为 0）
    int LuaInt(const char* name)
    {
        lua_getglobal(L, name);
        int value = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return value;
    }
};

// 同名函数挂在两个对象上：remove_call_out 只删发起者自己的那条
TEST_F(LuaCalloutTest, RemoveOnlyOwnEntries)
{
    CContainer a;
    CContainer b;
    HeartBeatGuard beat_a(&a);
    HeartBeatGuard beat_b(&b);
    a.set_name("callout_a");
    b.set_name("callout_b");

    ASSERT_TRUE(
        LoadScriptTable("callout_test_script = {\n"
                        "  test_scope_reset = function(self) end,\n"
                        "  test_scope_other = function(self) end\n"
                        "}"));

    ASSERT_GT(CallOut(&a, "test_scope_reset", 1), 0);
    ASSERT_GT(CallOut(&b, "test_scope_reset", 1), 0);
    ASSERT_GT(CallOut(&a, "test_scope_other", 1), 0);

    EXPECT_EQ(1, RemoveCallOut(&a, "test_scope_reset"));  // 只删 a 的
    EXPECT_EQ(1, RemoveCallOut(&b, "test_scope_reset"));  // b 的仍在
    EXPECT_EQ(1, RemoveCallOut(&a, "test_scope_other"));  // 异名不受影响
    EXPECT_EQ(0, RemoveCallOut(&a, "test_scope_reset"));  // a 已清空
}

// 注册期校验：按名注册不存在的函数 -> 立即拒绝（返回0），
// 不再等到执行期才 WARN。错误在离因最近处暴露。
TEST_F(LuaCalloutTest, RegisterRejectsMissingFunction)
{
    CContainer owner;
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    ASSERT_TRUE(LoadScriptTable("callout_test_script = {}"));

    EXPECT_EQ(0, CallOut(&owner, "no_such_function", 1));

    owner.heart_beat();  // 未注册成功，不应有任何回调执行
    EXPECT_EQ(0, LuaInt("callout_test_ran"));
}

// 函数值回调：模块定义的函数直接作为回调，注册、执行一条龙
TEST_F(LuaCalloutTest, FunctionValueExecutes)
{
    CContainer owner;
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    ASSERT_TRUE(LoadScriptTable("callout_test_script = {}"));
    ASSERT_EQ(LUA_OK, luaL_dostring(L,
                                    "callout_test_fn = function(self) "
                                    "callout_test_ran = 1 end"));

    ASSERT_GT(CallOutFunc(&owner, "callout_test_fn", 1), 0);

    // 函数值条目无名字，按名 remove 不影响它
    EXPECT_EQ(0, RemoveCallOut(&owner, "callout_test_fn"));

    owner.heart_beat();  // 延迟到点，执行回调
    EXPECT_EQ(1, LuaInt("callout_test_ran"));

    // 一次性回调：执行完条目已清理，再次心跳不重复执行
    owner.heart_beat();
    EXPECT_EQ(1, LuaInt("callout_test_ran"));
}

// 回调执行期间 remove_call_out 删自己：跳过，执行完由 execute_callout 清理
TEST_F(LuaCalloutTest, RemoveSkipsExecutingCallback)
{
    CContainer owner;
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    ASSERT_TRUE(
        LoadScriptTable("callout_test_script = {\n"
                        "  test_exec_reset = function(self)\n"
                        "    callout_test_ran = 1\n"
                        "    callout_test_removed = "
                        "self:remove_call_out(\"test_exec_reset\")\n"
                        "  end\n"
                        "}"));

    ASSERT_GT(CallOut(&owner, "test_exec_reset", 1), 0);

    owner.heart_beat();  // 延迟到点，执行回调

    EXPECT_EQ(1, LuaInt("callout_test_ran"));                // 回调确实执行了
    EXPECT_EQ(0, LuaInt("callout_test_removed"));            // 删自己 -> 跳过
    EXPECT_EQ(0, RemoveCallOut(&owner, "test_exec_reset"));  // 执行完清理
}

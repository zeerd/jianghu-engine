// test_lua_callout.cpp — Lua call_out 注册表契约
//
// 经 l_call_out / l_remove_call_out 真实绑定边界驱动 CLuaEngine：
// 1. remove_call_out 只删发起对象自己的挂起回调，跨对象同名函数互不影响；
// 2. 回调执行期间 remove_call_out 删自己会被跳过，执行完由 execute_callout
//    按 key 清理；
// 3. 按名注册的回退：owner 自己的脚本表缺名时回退到当前执行脚本（回调函数
//    属于发起 call_out 的脚本），两处都没有才拒绝注册。
#include "pch.h"

#include "Container.h"
#include "LuaBind.h"
#include "LuaBindDecls.h"
#include "LuaEngine.h"
#include "LuaNpc.h"
#include "LuaRoom.h"
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

// 缺名回退：owner 自己的脚本表没有该名字时，回退到当前执行脚本——
// 回调函数写在发起 call_out 的脚本里、对象只是效果作用目标的统一写法，
// 对玩家与脚本 NPC 都能按名注册（同一份效果代码，不分目标类型）。
TEST_F(LuaCalloutTest, NameFallsBackToIssuingScript)
{
    CLuaRoom owner("fixture_prefix_room");
    owner.create();  // 加载 fixture 脚本，拿到 owner 自己的脚本引用
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    // 发起脚本（active）：只有这张表里有回调；owner 自己的脚本（fixture）没有
    ASSERT_TRUE(
        LoadScriptTable("callout_test_script = {\n"
                        "  test_fallback_cb = function(self)\n"
                        "    callout_test_ran = 1\n"
                        "  end\n"
                        "}"));

    ASSERT_GT(CallOut(&owner, "test_fallback_cb", 1), 0)
        << "owner 脚本缺名时应回退到当前执行脚本";

    // 回退注册的条目同样能按名取消（同对象、同名字）
    EXPECT_EQ(1, RemoveCallOut(&owner, "test_fallback_cb"));

    // 重新注册并执行：执行时 self = owner 对象
    ASSERT_GT(CallOut(&owner, "test_fallback_cb", 1), 0);
    owner.heart_beat();
    EXPECT_EQ(1, LuaInt("callout_test_ran"));

    // 一次性：再次心跳不重复执行
    owner.heart_beat();
    EXPECT_EQ(1, LuaInt("callout_test_ran"));
}

// 名字两边都有：owner 自己的脚本表优先（回退只在缺名时发生）
TEST_F(LuaCalloutTest, OwnerScriptWinsOverIssuingScript)
{
    CLuaRoom owner("fixture_prefix_room");
    owner.create();
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    // 发起脚本也定义了同名 reset：必须执行 owner 脚本的版本（fixture 的
    // reset 给对象置 reset_ran），而不是发起脚本的版本（写全局
    // callout_test_ran）
    ASSERT_TRUE(
        LoadScriptTable("callout_test_script = {\n"
                        "  reset = function(self) callout_test_ran = 99 end\n"
                        "}"));

    ASSERT_GT(CallOut(&owner, "reset", 1), 0);
    owner.heart_beat();

    EXPECT_EQ(1, owner.query("reset_ran")) << "应执行 owner 脚本表里的版本";
    EXPECT_EQ(0, LuaInt("callout_test_ran")) << "不应执行发起脚本的同名版本";
}

// 两处都没有该名字：注册期拒绝（owner 有脚本、发起脚本也在，但都没有）
TEST_F(LuaCalloutTest, RejectsWhenNeitherScriptHasName)
{
    CLuaRoom owner("fixture_prefix_room");
    owner.create();
    HeartBeatGuard beat(&owner);
    owner.set_name("callout_owner");

    ASSERT_TRUE(LoadScriptTable("callout_test_script = {}"));

    EXPECT_EQ(0, CallOut(&owner, "no_such_function_anywhere", 1));
    owner.heart_beat();
    EXPECT_EQ(0, LuaInt("callout_test_ran"));
}

// 发起者销毁：按名回调定义在发起脚本、注册在目标身上——发起的 NPC 实例
// 销毁后，脚本表仍由引擎脚本缓存持有，到点照常执行且 self = 目标。
TEST_F(LuaCalloutTest, IssuerDestroyedCallbackStillFiresOnTarget)
{
    CContainer target;
    HeartBeatGuard beat_target(&target);
    target.set_name("callout_target");

    {
        CLuaNpc issuer("t_callout_issuer");
        issuer.create();  // 加载夹具脚本（脚本表进引擎缓存）
        const int issuer_ref = issuer.get_script_ref();
        ASSERT_NE(issuer_ref, LUA_NOREF);

        // 注册走「发起脚本」：目标无脚本 → 回退到当前执行脚本 = 发起脚本
        CLuaEngine::instance()->set_active_script_ref(issuer_ref);
        ASSERT_GT(CallOut(&target, "test_issuer_cb", 1), 0);
    }  // 发起者 NPC 实例销毁（脚本表不随之失效）

    target.heart_beat();  // 时达到点
    EXPECT_EQ(1, LuaInt("callout_test_ran")) << "发起者销毁后回调仍应执行";
    EXPECT_EQ(1, target.query("issuer_cb_mark")) << "执行时 self 必须是目标";
}

// 目标销毁：挂起的按名回调随析构释放、绝不触发；残留的注册表条目不影响
// 其他对象之后的同名注册与执行（object_id 单调不复用，残留不可达）。
TEST_F(LuaCalloutTest, DestroyedTargetReleasesNamedCallout)
{
    ASSERT_TRUE(
        LoadScriptTable("callout_test_script = {\n"
                        "  test_dead_cb = function(self)\n"
                        "    callout_test_ran = 1\n"
                        "  end\n"
                        "}"));

    CContainer* gone = new CContainer();
    gone->set_name("callout_gone");
    ASSERT_GT(CallOut(gone, "test_dead_cb", 1), 0);
    gone->set_heart_beat(0);
    delete gone;  // 目标销毁：队列随析构释放（契约：释放而非触发）

    EXPECT_EQ(0, LuaInt("callout_test_ran")) << "销毁不得触发回调";

    // 残留的注册表条目不影响其他对象的同名注册与执行
    CContainer other;
    HeartBeatGuard beat_other(&other);
    other.set_name("callout_other");
    ASSERT_GT(CallOut(&other, "test_dead_cb", 1), 0);
    other.heart_beat();
    EXPECT_EQ(1, LuaInt("callout_test_ran")) << "other 的同名注册照常执行";
}

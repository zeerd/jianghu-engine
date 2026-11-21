// test_bind_container_dbase.cpp — unit tests for the CMainObject/CContainer
// dbase & temp-mapping binds now is_living in object/ContainerDbase.cpp.
//
#include "test_bind_common.h"

#include "test_state_recorder.h"

class BindContainerDbaseTest : public BindTest {
protected:
    CChar ch;  // CChar is-a CContainer is-a CMainObject

    void PushStr(const char* s) { lua_pushstring(L, s); }
    void PushInt(long v) { lua_pushinteger(L, v); }
};

// ---------------- set / query / querystr / add / del ----------------
TEST_F(BindContainerDbaseTest, SetQueryInt)
{
    PushSelf(&ch);
    PushStr("level");
    PushInt(42);
    CallVoid(l_set, 3);

    PushSelf(&ch);
    PushStr("level");
    EXPECT_EQ(42, CallInt(l_query, 2));
}

TEST_F(BindContainerDbaseTest, SetQueryString)
{
    PushSelf(&ch);
    PushStr("owner");
    PushStr("huang");
    CallVoid(l_set, 3);

    PushSelf(&ch);
    PushStr("owner");
    std::string out;
    ASSERT_TRUE(CallStr(l_querystr, 2, out));
    EXPECT_EQ("huang", out);
}

TEST_F(BindContainerDbaseTest, AddAccumulates)
{
    ch.set("gold", (int32_t)10);
    PushSelf(&ch);
    PushStr("gold");
    PushInt(5);
    int k = call(l_add, 3);
    ASSERT_EQ(1, k) << "add 返回累加后的新值";
    EXPECT_EQ(15, (int)lua_tointeger(L, -1));
    lua_pop(L, k);
    EXPECT_EQ(15, ch.query("gold"));
}

// 守卫：obj 为空的绑定 userdata（对象已销毁但 Lua 仍持有）-> 返回 0（不是 nil）
TEST_F(BindContainerDbaseTest, AddGuardReturnsZero)
{
    LuaObject* ud = (LuaObject*)lua_newuserdata(L, sizeof(LuaObject));
    ud->obj       = NULL;
    luaL_getmetatable(L, "jianghu_object");
    lua_setmetatable(L, -2);

    lua_pushstring(L, "gold");
    lua_pushinteger(L, 5);
    int k = call(l_add, 3);
    ASSERT_EQ(1, k) << "add 守卫路径也压值";
    EXPECT_EQ(0, (int)lua_tointeger(L, -1));
    lua_pop(L, k);
}

TEST_F(BindContainerDbaseTest, Del)
{
    ch.set("k", (int32_t)7);
    PushSelf(&ch);
    PushStr("k");
    CallVoid(l_del, 2);
    EXPECT_EQ(0, ch.query("k"));
}

// ---------------- temp（set/query/add/del 的 "T" 作用域） ----------------
TEST_F(BindContainerDbaseTest, SetQueryTemp)
{
    PushSelf(&ch);
    PushStr("t/x");
    PushInt(9);
    PushStr("T");
    CallVoid(l_set, 4);

    PushSelf(&ch);
    PushStr("t/x");
    PushStr("T");
    EXPECT_EQ(9, CallInt(l_query, 3));
    // 同键持久袋不受影响：两袋隔离
    EXPECT_EQ(0, ch.query("t/x"));

    // 字符串值与数字同支
    PushSelf(&ch);
    PushStr("t/s");
    PushStr("abc");
    PushStr("T");
    CallVoid(l_set, 4);
    EXPECT_STREQ("abc", ch.querystr_temp("t/s"));
}

TEST_F(BindContainerDbaseTest, QuerystrTemp)
{
    ch.set_temp("t/s", "abc");
    PushSelf(&ch);
    PushStr("str");
    PushStr("t/s");
    std::string out;
    ASSERT_TRUE(CallStr(l_dbase_temp, 3, out));
    EXPECT_EQ("abc", out);
}

TEST_F(BindContainerDbaseTest, AddTempAndDelete)
{
    ch.add_temp("cnt", 3);
    PushSelf(&ch);
    PushStr("cnt");
    PushInt(4);
    PushStr("T");
    int k = call(l_add, 4);
    ASSERT_EQ(1, k) << "add_temp 返回累加后的新值";
    EXPECT_EQ(7, (int)lua_tointeger(L, -1));
    lua_pop(L, k);
    EXPECT_EQ(7, ch.query_temp("cnt"));

    PushSelf(&ch);
    PushStr("cnt");
    PushStr("T");
    CallVoid(l_del, 3);
    EXPECT_EQ(0, ch.query_temp("cnt"));
}

// "T" 之外的作用域拒绝：写错落点（持久/临时）比拒绝更危险
TEST_F(BindContainerDbaseTest, UnknownScopeErrors)
{
    PushSelf(&ch);
    PushStr("k");
    PushInt(1);
    PushStr("X");
    EXPECT_EQ(-1, call(l_set, 4));

    PushSelf(&ch);
    PushStr("k");
    PushStr("X");
    EXPECT_EQ(-1, call(l_query, 3));

    PushSelf(&ch);
    PushStr("k");
    PushInt(1);
    PushStr("X");
    EXPECT_EQ(-1, call(l_add, 4));

    PushSelf(&ch);
    PushStr("k");
    PushStr("X");
    EXPECT_EQ(-1, call(l_del, 3));
}

// ---------------- name / id ----------------
TEST_F(BindContainerDbaseTest, NameAndId)
{
    ch.set("name", "meimei");
    ch.set("id", "mm1");
    PushSelf(&ch);
    std::string n;
    ASSERT_TRUE(CallStr(l_name, 1, n));
    EXPECT_EQ("meimei", n);

    PushSelf(&ch);
    std::string i;
    ASSERT_TRUE(CallStr(l_id, 1, i));
    EXPECT_EQ("mm1", i);
}

// ---------------- set_name：改名后向同容器玩家重发行包 ----------------
// 协议没有「改名」消息，名字只在物件进场景时下发；不在这里重发的话，
// 已在场玩家拿到的永远是旧名字。
class BindDbaseRenameTest : public BindEngineTest {
protected:
    void PushStr(const char* s) { lua_pushstring(L, s); }
};

TEST_F(BindDbaseRenameTest, SetNamePushesRenameToRoomViewer)
{
    CContainer room;
    StateRecordingUser viewer;
    viewer.set_name("viewer", "bd1");
    viewer.move(&room);
    viewer.clear();  // 隔离入房时的 11+17 下发

    CItem item;
    item.set_name("旧名", "bditem");
    item.move(&room);
    viewer.clear();  // 隔离入场景时的旧名下发

    PushSelf(&item);
    PushStr("新名");
    PushStr("bditem");
    CallVoid(l_set_name, 3);

    EXPECT_TRUE(viewer.has("N=新名"));
    EXPECT_TRUE(viewer.has("I=bditem"));
}

// ---------------- dbase(action, key) / dbase_temp(action, key)
// ----------------
TEST_F(BindContainerDbaseTest, QuerymapAndKeys)
{
    CMapping m;
    m.set("a", (int32_t)1);
    m.set("b", (int32_t)2);
    ch.set("sub", m);

    PushSelf(&ch);
    PushStr("map");
    PushStr("sub");
    EXPECT_TRUE(CallBool(l_dbase, 3));  // exists -> true

    PushSelf(&ch);
    PushStr("map");
    PushStr("nosuch");
    EXPECT_EQ(1, CallRetCount(l_dbase, 3));  // returns nil (1 value)

    PushSelf(&ch);
    PushStr("map_keys");
    PushStr("sub");
    EXPECT_EQ(1, CallRetCount(l_dbase, 3));
}

TEST_F(BindContainerDbaseTest, QuerymapTempFamily)
{
    CMapping m;
    m.set("x", (int32_t)5);
    ch.set_temp("tmp", m);

    PushSelf(&ch);
    PushStr("map");
    PushStr("tmp");
    EXPECT_EQ(1, CallRetCount(l_dbase_temp, 3));

    PushSelf(&ch);
    PushStr("map_keys");
    PushStr("tmp");
    EXPECT_EQ(1, CallRetCount(l_dbase_temp, 3));
}

TEST_F(BindContainerDbaseTest, DbaseDefined)
{
    ch.set("present", (int32_t)1);
    PushSelf(&ch);
    PushStr("defined");
    PushStr("present");
    EXPECT_TRUE(CallBool(l_dbase, 3));

    PushSelf(&ch);
    PushStr("defined");
    PushStr("absent");
    EXPECT_FALSE(CallBool(l_dbase, 3));
}

TEST_F(BindContainerDbaseTest, UnknownActionErrors)
{
    PushSelf(&ch);
    PushStr("nope");
    EXPECT_EQ(-1, call(l_dbase, 2));

    PushSelf(&ch);
    PushStr("nope");
    EXPECT_EQ(-1, call(l_dbase_temp, 2));
}

// ---------------- dbase("get") / dbase("set") ----------------
TEST_F(BindContainerDbaseTest, SetMapGetMap)
{
    // build a lua table {hp=10, mp="hi"} and call dbase("set", "bag", tbl)
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "set");
    lua_pushstring(L, "bag");
    lua_newtable(L);
    lua_pushinteger(L, 10);
    lua_setfield(L, -2, "hp");
    lua_pushstring(L, "hi");
    lua_setfield(L, -2, "mp");
    CallVoid(l_dbase, 4);

    CMapping* got = ch.querymap("bag");
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(10, got->query("hp"));
    EXPECT_STREQ("hi", got->querystr("mp"));
}

TEST_F(BindContainerDbaseTest, GetMapReturnsTable)
{
    CMapping m;
    m.set("k", (int32_t)3);
    ch.set("mm", m);
    PushSelf(&ch);
    PushStr("get");
    PushStr("mm");
    EXPECT_EQ(1, CallRetCount(l_dbase, 3));
}

// ---------------- queryvec / set_beast_verbs ----------------
static void PushStrArray(lua_State* L, const char* const* items, int n)
{
    lua_newtable(L);
    for (int i = 0; i < n; i++) {
        lua_pushstring(L, items[i]);
        lua_rawseti(L, -2, i + 1);
    }
}

TEST_F(BindContainerDbaseTest, SetBeastVerbsStoresWhitelist)
{
    // 白名单来自 data/beast_combat_action.lua，校验要走引擎
    ASSERT_NE(ensure_lua_env(), nullptr);

    // self:set_beast_verbs({"bite", "claw"})
    PushSelf(&ch);
    const char* verbs[] = {"bite", "claw"};
    PushStrArray(L, verbs, 2);
    CallVoid(l_set_beast_verbs, 2);

    CVector* v = ch.queryvec("verbs");
    ASSERT_NE(nullptr, v);
    ASSERT_EQ(2, (int)v->count());
    EXPECT_STREQ("bite", (*v)[0]);
    EXPECT_STREQ("claw", (*v)[1]);

    // queryvec -> lua table
    PushSelf(&ch);
    PushStr("vec");
    PushStr("verbs");
    EXPECT_EQ(1, CallRetCount(l_dbase, 3));
}

TEST_F(BindContainerDbaseTest, SetBeastVerbsRejectsUnknownVerb)
{
    // 白名单校验失败要抛 Lua 错误，且不得留下半成品 dbase。
    // 引擎在位才能区分「表外动词被拒」与「动作表读不到」，否则整批动词
    // 都会被拒，用例等于没验。
    ASSERT_NE(ensure_lua_env(), nullptr);

    PushSelf(&ch);
    const char* verbs[] = {"bite", "bogus"};
    PushStrArray(L, verbs, 2);
    EXPECT_EQ(-1, call(l_set_beast_verbs, 2));
    EXPECT_EQ(nullptr, ch.queryvec("verbs"));
}

TEST_F(BindContainerDbaseTest, SetBeastVerbsEmptyTable)
{
    // 空表等价于未设置：queryvec 读出 nil -> 战斗走默认攻击文案
    PushSelf(&ch);
    PushStrArray(L, nullptr, 0);
    CallVoid(l_set_beast_verbs, 2);

    PushSelf(&ch);
    PushStr("vec");
    PushStr("verbs");
    EXPECT_EQ(1, CallRetCount(l_dbase, 3));
}

// ---------------- object_id / create / set_heart_beat (Container.cpp)
// ----------------
TEST_F(BindContainerDbaseTest, ObjectId)
{
    PushSelf(&ch);
    int64_t id = CallInt(l_object_id, 1);
    EXPECT_GT(id, 0);
}

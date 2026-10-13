// test_bind_container_dbase.cpp — unit tests for the CMainObject/CContainer
// dbase & temp-mapping binds now living in object/ContainerDbase.cpp.
//
#include "test_bind_common.h"

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
    CallVoid(l_add, 3);
    EXPECT_EQ(15, ch.query("gold"));
}

TEST_F(BindContainerDbaseTest, Del)
{
    ch.set("k", (int32_t)7);
    PushSelf(&ch);
    PushStr("k");
    CallVoid(l_del, 2);
    EXPECT_EQ(0, ch.query("k"));
}

// ---------------- temp ----------------
TEST_F(BindContainerDbaseTest, SetQueryTemp)
{
    PushSelf(&ch);
    PushStr("t/x");
    PushInt(9);
    CallVoid(l_set_temp, 3);

    PushSelf(&ch);
    PushStr("t/x");
    EXPECT_EQ(9, CallInt(l_query_temp, 2));
}

TEST_F(BindContainerDbaseTest, QuerystrTemp)
{
    ch.set_temp("t/s", "abc");
    PushSelf(&ch);
    PushStr("t/s");
    std::string out;
    ASSERT_TRUE(CallStr(l_querystr_temp, 2, out));
    EXPECT_EQ("abc", out);
}

TEST_F(BindContainerDbaseTest, AddTempAndDelete)
{
    ch.add_temp("cnt", 3);
    PushSelf(&ch);
    PushStr("cnt");
    PushInt(4);
    CallVoid(l_add_temp, 3);
    EXPECT_EQ(7, ch.query_temp("cnt"));

    PushSelf(&ch);
    PushStr("cnt");
    CallVoid(l_delete_temp, 2);
    EXPECT_EQ(0, ch.query_temp("cnt"));
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

// ---------------- querymap family ----------------
TEST_F(BindContainerDbaseTest, QuerymapAndKeys)
{
    CMapping m;
    m.set("a", (int32_t)1);
    m.set("b", (int32_t)2);
    ch.set("sub", m);

    PushSelf(&ch);
    PushStr("sub");
    EXPECT_TRUE(CallBool(l_querymap, 2));  // exists -> true

    PushSelf(&ch);
    PushStr("nosuch");
    EXPECT_EQ(1, CallRetCount(l_querymap, 2));  // returns nil (1 value)

    PushSelf(&ch);
    PushStr("sub");
    EXPECT_EQ(1, CallRetCount(l_querymap_keys, 2));
}

TEST_F(BindContainerDbaseTest, QuerymapTempFamily)
{
    CMapping m;
    m.set("x", (int32_t)5);
    ch.set_temp("tmp", m);

    PushSelf(&ch);
    PushStr("tmp");
    EXPECT_EQ(1, CallRetCount(l_querymap_temp, 2));

    PushSelf(&ch);
    PushStr("tmp");
    EXPECT_EQ(1, CallRetCount(l_querymap_temp_keys, 2));
}

TEST_F(BindContainerDbaseTest, DbaseDefined)
{
    ch.set("present", (int32_t)1);
    PushSelf(&ch);
    PushStr("present");
    EXPECT_TRUE(CallBool(l_dbase_defined, 2));

    PushSelf(&ch);
    PushStr("absent");
    EXPECT_FALSE(CallBool(l_dbase_defined, 2));
}

// ---------------- get_map / set_map ----------------
TEST_F(BindContainerDbaseTest, SetMapGetMap)
{
    // build a lua table {hp=10, mp="hi"} and call set_map("bag", tbl)
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "bag");
    lua_newtable(L);
    lua_pushinteger(L, 10);
    lua_setfield(L, -2, "hp");
    lua_pushstring(L, "hi");
    lua_setfield(L, -2, "mp");
    CallVoid(l_set_map, 3);

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
    PushStr("mm");
    EXPECT_EQ(1, CallRetCount(l_get_map, 2));
}

// ---------------- queryvec / set_verbs ----------------
TEST_F(BindContainerDbaseTest, SetVerbsAndQueryvec)
{
    // set_verbs("verbs", "bite", "claw")
    lua_bind_push_object(L, &ch);
    lua_pushstring(L, "verbs");
    lua_pushstring(L, "bite");
    lua_pushstring(L, "claw");
    CallVoid(l_set_verbs, 4);

    CVector* v = ch.queryvec("verbs");
    ASSERT_NE(nullptr, v);
    ASSERT_EQ(2, (int)v->count());
    EXPECT_STREQ("bite", (*v)[0]);
    EXPECT_STREQ("claw", (*v)[1]);

    // queryvec -> lua table
    PushSelf(&ch);
    PushStr("verbs");
    EXPECT_EQ(1, CallRetCount(l_queryvec, 2));

    PushSelf(&ch);
    PushStr("emptyvec");
    EXPECT_EQ(1, CallRetCount(l_queryvec, 2));  // nil
}

// ---------------- object_id / create / set_heart_beat (Container.cpp)
// ----------------
TEST_F(BindContainerDbaseTest, ObjectId)
{
    PushSelf(&ch);
    int64_t id = CallInt(l_object_id, 1);
    EXPECT_GT(id, 0);
}

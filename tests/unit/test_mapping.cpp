// test_mapping.cpp - Unit tests for CMapping / CVector (standard-container
// rewrite).
//
// Migrated from test/test_mapping.cpp to Google Test.
// Build:  msbuild tests.vcxproj /p:Configuration=Debug /p:Platform=Win32
// Run:    tests\Debug\tests.exe
//
// Notes:
//   - Uses forward-declared stubs; real implementation compiled as Mapping.cpp
//     in the same project. Linking provides the actual symbols.
//   - log_message() is now provided by Logger.cpp (compiled in the project).
//
#include <cstdarg>
#include <cstdio>
#include "Logger.h"

#include "gtest/gtest.h"
#include "Mapping.h"

// ---------------------------------------------------------------------------
// CVector basics
// ---------------------------------------------------------------------------
TEST(CVector, Append_Unique)
{
    CVector v;
    v.append("a");
    v.append("b");
    v.append("a");  // duplicate ignored
    EXPECT_EQ(v.count(), 2);
    EXPECT_NE(v.find("a"), v.end());
    EXPECT_EQ(v.find("c"), v.end());
}

TEST(CVector, remove)
{
    CVector v;
    v.append("x");
    v.append("y");
    v.remove("x");
    EXPECT_EQ(v.count(), 1);
    EXPECT_STREQ(v[0], "y");
    v.remove("nope");  // removing non-existent is a no-op
    EXPECT_EQ(v.count(), 1);
}

TEST(CVector, OperatorIndex_Boundaries)
{
    CVector v;
    v.append("p");
    // out-of-range must return ""
    EXPECT_STREQ(v[1], "");
    EXPECT_STREQ(v[99], "");
    EXPECT_STREQ(v[0], "p");
}

TEST(CVector, Buffer_RoundTrip)
{
    CVector v;
    v.append("chat");
    v.append("rumor");
    const char* buf = v.get_buffer();
    // JSON 数组格式
    EXPECT_STREQ(buf, "[\"chat\",\"rumor\"]");

    CVector w;
    long n = w.load_buffer(buf);
    EXPECT_GT(n, 0L);
    EXPECT_EQ(w.count(), 2);
    EXPECT_STREQ(w[0], "chat");
    EXPECT_STREQ(w[1], "rumor");

    // re-serialize -> identical
    EXPECT_STREQ(w.get_buffer(), buf);
}

// ---------------------------------------------------------------------------
// CMapping: basic CRUD
// ---------------------------------------------------------------------------
TEST(CMapping, Basic_SetQuery)
{
    CMapping m;
    m.set("name", "azhu");
    m.set("age", 20);
    m.set("hp", 1000);

    EXPECT_STREQ(m.querystr("name"), "azhu");
    EXPECT_EQ(m.query("age"), 20);
    EXPECT_EQ(m.query("hp"), 1000);
    EXPECT_EQ(m.query("missing"), 0);  // missing -> 0
    EXPECT_STREQ(m.querystr("missing"), "");
    EXPECT_EQ(m.get_count(), 3);
}

TEST(CMapping, Overwrite_SameKey)
{
    CMapping m;
    m.set("k", 1);
    EXPECT_EQ(m.query("k"), 1);
    m.set("k", 2);  // number -> number
    EXPECT_EQ(m.query("k"), 2);
    m.set("k", "now a string");  // number -> string
    EXPECT_STREQ(m.querystr("k"), "now a string");
    EXPECT_EQ(m.query("k"), 0);  // type changed, query() returns 0
    m.set("k", 7);               // string -> number
    EXPECT_EQ(m.query("k"), 7);
    EXPECT_EQ(m.get_count(), 1);  // still one entry
}

TEST(CMapping, Add)
{
    CMapping m;
    EXPECT_EQ(m.add("score", 5), 5);
    EXPECT_EQ(m.add("score", 7), 12);  // accumulate
    EXPECT_EQ(m.query("score"), 12);
    EXPECT_EQ(m.add("other", 3), 3);
    EXPECT_EQ(m.get_count(), 2);
}

TEST(CMapping, Undefinedp)
{
    CMapping m;
    m.set("a", 1);
    EXPECT_EQ(m.undefinedp("a"), 0);
    EXPECT_EQ(m.undefinedp("b"), 1);
}

TEST(CMapping, Del)
{
    CMapping m;
    m.set("a", 1);
    m.set("b", 2);
    m.del("a");
    EXPECT_EQ(m.undefinedp("a"), 1);
    EXPECT_EQ(m.undefinedp("b"), 0);
    EXPECT_EQ(m.get_count(), 1);
    m.del("nope");  // no-op
    EXPECT_EQ(m.get_count(), 1);
}

// ---------------------------------------------------------------------------
// CMapping: nested paths ("a/b/c")
// ---------------------------------------------------------------------------
TEST(CMapping, Nested_Path)
{
    CMapping m;
    m.set("a/b/c", 42);
    EXPECT_EQ(m.query("a/b/c"), 42);

    CMapping* sub = m.querymap("a/b");
    ASSERT_NE(sub, (CMapping*)NULL);
    EXPECT_EQ(sub->query("c"), 42);
    EXPECT_NE(m.querymap("a"),
              (CMapping*)NULL);  // intermediate node is a mapping
    EXPECT_EQ(m.query("a"), 0);

    // nested string
    m.set("x/y", "deep");
    EXPECT_STREQ(m.querystr("x/y"), "deep");
    // x itself holds a mapping, not a string
    EXPECT_STREQ(m.querystr("x"), "");

    // delete through a path
    m.del("a/b/c");
    EXPECT_EQ(m.query("a/b/c"), 0);
    EXPECT_EQ(m.querymap("a/b")->get_count(), 0);
}

// ---------------------------------------------------------------------------
// CMapping: copyMapping deep copy
// ---------------------------------------------------------------------------
TEST(CMapping, CopyMapping_Deep)
{
    CMapping src;
    src.set("name", "fae");
    src.set("lv", 30);
    CVector vec;
    vec.append("sword");
    vec.append("cuff");
    src.set("skills", vec);
    src.set("attr/str", 25);  // nested map

    CMapping dst;
    dst.set("name", "other");
    dst.copy_mapping(&src);

    EXPECT_STREQ(dst.querystr("name"), "fae");
    EXPECT_EQ(dst.query("lv"), 30);
    CVector* sv = dst.queryvec("skills");
    ASSERT_NE(sv, (CVector*)NULL);
    EXPECT_EQ(sv->count(), 2);
    EXPECT_EQ(dst.query("attr/str"), 25);

    // deep: mutating src must not affect dst
    src.set("name", "changed");
    src.set("attr/str", 1);
    EXPECT_STREQ(dst.querystr("name"), "fae");
    EXPECT_EQ(dst.query("attr/str"), 25);
}

// ---------------------------------------------------------------------------
// CMapping: serialization round-trip (JSON object format)
// ---------------------------------------------------------------------------
TEST(CMapping, GetBuffer_LoadBuffer_RoundTrip)
{
    CMapping m;
    m.set("age", 20);
    m.set("name", "xiao");
    m.set("quote", "say \"hi\"");
    m.set("newline", "line1\nline2");
    m.set("flag", 1);
    CVector vec;
    vec.append("a");
    vec.append("b");
    m.set("vec", vec);
    m.set("nested/deep", 7);

    const char* buf = m.get_buffer();

    CMapping n;
    long n2 = n.load_buffer(buf);
    EXPECT_GT(n2, 0L);
    EXPECT_EQ(n.query("age"), 20);
    EXPECT_STREQ(n.querystr("name"), "xiao");
    EXPECT_STREQ(n.querystr("quote"), "say \"hi\"");
    // JSON preserves '\n' losslessly (the legacy one-way \n->\r is gone)
    EXPECT_STREQ(n.querystr("newline"), "line1\nline2");
    EXPECT_EQ(n.query("flag"), 1);
    CVector* v = n.queryvec("vec");
    ASSERT_NE(v, (CVector*)NULL);
    EXPECT_EQ(v->count(), 2);
    EXPECT_EQ(n.query("nested/deep"), 7);
}

TEST(CMapping, LoadBuffer_HandlesNumberSign)
{
    CMapping m;
    EXPECT_GT(m.load_buffer("{\"neg\":-5,\"pos\":3}"), 0L);
    EXPECT_EQ(m.query("neg"), -5);
    EXPECT_EQ(m.query("pos"), 3);
}

TEST(CMapping, GetBuffer_Empty)
{
    CMapping m;
    const char* buf = m.get_buffer();
    // empty mapping serializes as {}
    EXPECT_STREQ(buf, "{}");
    CMapping n;
    EXPECT_GT(n.load_buffer(buf), 0L);
    EXPECT_EQ(n.get_count(), 0);
}

// ---------------------------------------------------------------------------
// CMapping: keys / values
// ---------------------------------------------------------------------------
TEST(CMapping, Keys_Values)
{
    CMapping m;
    m.set("a", 1);
    m.set("b", "str");
    m.set("c", 3);
    CVector k, vv;
    m.keys(k);
    m.values(vv);
    EXPECT_EQ(k.count(), 3);
    EXPECT_EQ(vv.count(), 1);
    EXPECT_STREQ(vv[0], "str");
    EXPECT_NE(k.find("a"), k.end());
}

// ---------------------------------------------------------------------------
// CMapping: explicit nested-mapping round-trip (T_MAPPING value via set(key,
// CMapping&)). This exercises the nested-object branch of the JSON codec —
// the highest-risk path for the svalue_t ownership refactor.
// ---------------------------------------------------------------------------
TEST(CMapping, NestedMapping_RoundTrip)
{
    CMapping inner;
    inner.set("hp", 100);
    inner.set("name", "sub");
    CVector iv;
    iv.append("x");
    iv.append("y");
    inner.set("list", iv);

    CMapping outer;
    outer.set("child", inner);  // T_MAPPING value
    outer.set("top", 1);

    const char* buf = outer.get_buffer();

    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L);
    CMapping* c = back.querymap("child");
    ASSERT_NE(c, (CMapping*)NULL);
    EXPECT_EQ(c->query("hp"), 100);
    EXPECT_STREQ(c->querystr("name"), "sub");
    CVector* v = c->queryvec("list");
    ASSERT_NE(v, (CVector*)NULL);
    EXPECT_EQ(v->count(), 2);
    EXPECT_STREQ((*v)[0], "x");
    EXPECT_STREQ((*v)[1], "y");
    EXPECT_EQ(back.query("top"), 1);
}

// ---------------------------------------------------------------------------
// CMapping: repeated overwrite of one key across all value types. free_svalue
// must release the previous value each time; after the refactor unique_ptr does
// this automatically. Verify no corruption and correct final value.
// ---------------------------------------------------------------------------
TEST(CMapping, Overwrite_AllTypes)
{
    CMapping m;
    m.set("k", "a string");
    EXPECT_STREQ(m.querystr("k"), "a string");

    // string -> nested map (overwrites, must free the old string)
    CMapping sub;
    sub.set("inner", 7);
    m.set("k", sub);
    ASSERT_NE(m.querymap("k"), (CMapping*)NULL);
    EXPECT_EQ(m.querymap("k")->query("inner"), 7);

    // map -> array (must free the old sub-map)
    CVector vec;
    vec.append("one");
    m.set("k", vec);
    ASSERT_NE(m.queryvec("k"), (CVector*)NULL);
    EXPECT_EQ(m.queryvec("k")->count(), 1);

    // array -> number (must free the old vector)
    m.set("k", 99);
    EXPECT_EQ(m.query("k"), 99);
    EXPECT_EQ(m.querymap("k"), (CMapping*)NULL);

    // number -> string (round-trips through serialization)
    m.set("k", "final");
    CMapping back;
    ASSERT_GT(back.load_buffer(m.get_buffer()), 0L);
    EXPECT_STREQ(back.querystr("k"), "final");
}

// ---------------------------------------------------------------------------
// CMapping: deep mixed nesting (map -> map -> array + strings) round-trip.
// ---------------------------------------------------------------------------
TEST(CMapping, DeepMixed_RoundTrip)
{
    CMapping leaf;
    leaf.set("a", "aval");
    leaf.set("b", 2);

    CMapping mid;
    mid.set("leaf", leaf);
    CVector mv;
    mv.append("p");
    mv.append("q");
    mv.append("r");
    mid.set("arr", mv);

    CMapping root;
    root.set("mid", mid);

    const char* buf = root.get_buffer();
    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L);
    CMapping* m = back.querymap("mid");
    ASSERT_NE(m, (CMapping*)NULL);
    CMapping* l = m->querymap("leaf");
    ASSERT_NE(l, (CMapping*)NULL);
    EXPECT_STREQ(l->querystr("a"), "aval");
    EXPECT_EQ(l->query("b"), 2);
    CVector* v = m->queryvec("arr");
    ASSERT_NE(v, (CVector*)NULL);
    EXPECT_EQ(v->count(), 3);
    EXPECT_STREQ((*v)[2], "r");
}

// ---------------------------------------------------------------------------
// Serialization change-surface anchors.
//
// These pin the behaviour of the (["k":v,]) codec that the JSON rewrite
// replaces, so every reachable line of the old save/load paths was executed
// before the switch (anchor tests captured during Phase 0).
// ---------------------------------------------------------------------------

// JSON escapes '"' and '\' inside keys and decodes them on read, so keys
// round-trip exactly (the legacy reader never unescaped keys — a bug the
// rewrite fixes).
TEST(CMapping, RoundTrip_QuotesInKey)
{
    CMapping m;
    m.set("say\"hi", 1);

    const char* buf = m.get_buffer();
    EXPECT_NE(nullptr, strstr(buf, "\\\"")) << "buffer=" << buf;

    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_EQ(back.query("say\"hi"), 1) << "buffer=" << buf;
}

TEST(CMapping, RoundTrip_BackslashInKey)
{
    CMapping m;
    m.set("path\\to", 2);  // one real backslash in the key

    const char* buf = m.get_buffer();
    EXPECT_NE(nullptr, strstr(buf, "\\\\")) << "buffer=" << buf;

    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_EQ(back.query("path\\to"), 2) << "buffer=" << buf;
}

// Value side of the same escaping rules, plus '/' which a JSON writer escapes.
TEST(CMapping, RoundTrip_SpecialCharsInValue)
{
    CMapping m;
    m.set("q", "a\"b");
    m.set("b", "a\\b");
    m.set("s", "a/b");
    m.set("mixed", "\\\"/\\\n");

    const char* buf = m.get_buffer();

    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_STREQ(back.querystr("q"), "a\"b");
    EXPECT_STREQ(back.querystr("b"), "a\\b");
    EXPECT_STREQ(back.querystr("s"), "a/b");
    // JSON keeps '\n' verbatim (no more lossy \n->\r normalisation)
    EXPECT_STREQ(back.querystr("mixed"), "\\\"/\\\n");
}

// High-bit bytes (game strings are GBK) must pass through untouched. GBK trail
// bytes include 0x5C ('\'), which is the byte most likely to corrupt a codec.
TEST(CMapping, RoundTrip_GbkTrailByteBackslash)
{
    // 0x81 0x5C is a legal GBK double-byte character whose trail byte is '\'.
    char gbk[4];
    gbk[0] = static_cast<char>(0x81);
    gbk[1] = static_cast<char>(0x5C);
    gbk[2] = static_cast<char>(0xA0);
    gbk[3] = 0;

    // 0xC4 0xE3 = "ni" in GBK — ordinary high-bit text with no special byte.
    char plain[3];
    plain[0] = static_cast<char>(0xC4);
    plain[1] = static_cast<char>(0xE3);
    plain[2] = 0;

    CMapping m;
    m.set("cn", plain);
    m.set("gbk", gbk);
    m.set("flag", 7);

    const char* buf = m.get_buffer();

    CMapping back;
    ASSERT_GT(back.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_STREQ(back.querystr("cn"), plain);
    ASSERT_EQ(strlen(back.querystr("gbk")), 3u) << "buffer=" << buf;
    EXPECT_EQ(static_cast<unsigned char>(back.querystr("gbk")[0]), 0x81);
    EXPECT_EQ(static_cast<unsigned char>(back.querystr("gbk")[1]), 0x5C);
    EXPECT_EQ(static_cast<unsigned char>(back.querystr("gbk")[2]), 0xA0);
    EXPECT_EQ(back.query("flag"), 7);
}

// Array elements go through CVector's own writer, which has no escaping of its
// own for '"' — pin what the codec must keep working for ordinary elements.
TEST(CVector, RoundTrip_SlashElements)
{
    CVector v;
    v.append("a/b");
    v.append("room/void/1");
    const char* buf = v.get_buffer();

    CVector w;
    ASSERT_GT(w.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_EQ(w.count(), 2);
    EXPECT_STREQ(w[0], "a/b");
    EXPECT_STREQ(w[1], "room/void/1");
}

// 读入合法 JSON，但某些键的值是 null/布尔/浮点（本写侧永不产出的类型）：
// from_json_object 走 default 跳过这些键，其余正常入库。
TEST(CMapping, LoadBuffer_SkipsNonCanonicalTypes)
{
    CMapping m;
    ASSERT_GT(
        m.load_buffer("{\"a\":true,\"b\":null,\"c\":1.5,\"d\":1,\"e\":\"ok\"}"),
        0L);
    // 非本格式类型被跳过
    EXPECT_EQ(m.undefinedp("a"), 1);
    EXPECT_EQ(m.undefinedp("b"), 1);
    EXPECT_EQ(m.undefinedp("c"), 1);
    // 规范类型正常读入
    EXPECT_EQ(m.query("d"), 1);
    EXPECT_STREQ(m.querystr("e"), "ok");
    EXPECT_EQ(m.get_count(), 2);
}

// str_wiz_only 在 CmdUserAccount::set_param 里喂给 CVector::load_buffer，
// 用于拒绝非巫师设置 invisibility/immortal。锁定该 JSON 字面量确实解析成
// 含这两个元素的数组（否则守卫会静默失效）。
TEST(CVector, LoadBuffer_WizOnlyLiteral)
{
    CVector v;
    ASSERT_GT(v.load_buffer("[\"invisibility\",\"immortal\"]"), 0L);
    EXPECT_EQ(v.count(), 2);
    EXPECT_NE(v.find("invisibility"), v.end());
    EXPECT_NE(v.find("immortal"), v.end());
    EXPECT_EQ(v.find("wimpy"), v.end());
}

// Duplicate elements must not survive into the array: CVector keeps an ordered
// unique set, so a reader has to de-duplicate whatever the wire format allows.
TEST(CVector, LoadBuffer_Deduplicates)
{
    CVector v;
    v.append("x");
    v.append("y");
    v.append("x");
    EXPECT_EQ(v.count(), 2);

    const char* buf = v.get_buffer();
    CVector w;
    ASSERT_GT(w.load_buffer(buf), 0L);
    EXPECT_EQ(w.count(), 2);
    w.append("y");
    EXPECT_EQ(w.count(), 2);  // index stays in sync with the array
}

// Every rejection path of CMapping's JSON reader.
TEST(CMapping, LoadBuffer_RejectsMalformedInput)
{
    static const char* const bad[] = {
        "x",            // not a JSON value
        "{",            // unterminated object
        "{\"k\"}",      // key without ':' and value
        "{\"k\":}",     // missing value
        "{,}",          // object body starts with ','
        "{\"k\":tru}",  // malformed boolean
        "[1,2]",        // array is not a mapping
        "123",          // bare number is not a mapping
        "\"str\"",      // bare string is not a mapping
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        CMapping m;
        EXPECT_EQ(m.load_buffer(bad[i]), -1L) << "input=" << bad[i];
    }

    // truncated input: the loop runs out of characters
    CMapping m;
    EXPECT_EQ(m.load_buffer(""), -1L);
    EXPECT_EQ(m.load_buffer("{\"k\":1"), -1L);
    EXPECT_EQ(m.load_buffer("{\"k\""), -1L);
    EXPECT_EQ(m.load_buffer("{"), -1L);
    EXPECT_EQ(m.load_buffer("{\"nested\":{\"a\":1"), -1L);
}

// Every rejection path of CVector's JSON reader.
TEST(CVector, LoadBuffer_RejectsMalformedInput)
{
    static const char* const bad[] = {
        "x",      // not a JSON value
        "[",      // unterminated array
        "[1,]",   // dangling comma
        "[a,b]",  // element is not quoted
        "{}",     // object is not an array
        "123",    // bare number is not an array
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        CVector v;
        EXPECT_EQ(v.load_buffer(bad[i]), -1L) << "input=" << bad[i];
    }

    // truncated input
    CVector t;
    EXPECT_EQ(t.load_buffer(""), -1L);
    EXPECT_EQ(t.load_buffer("[\"a\""), -1L);
    EXPECT_EQ(t.load_buffer("["), -1L);

    // valid empty array
    CVector v;
    EXPECT_GT(v.load_buffer("[]"), 0L);
    EXPECT_EQ(v.count(), 0);
}

// JSON writer escapes quotes/backslashes inside array elements (the legacy
// CVector writer did not), so such elements now round-trip.
TEST(CVector, RoundTrip_SpecialCharElements)
{
    CVector v;
    v.append("a\"b");
    v.append("c\\d");
    v.append("e/f");

    const char* buf = v.get_buffer();
    EXPECT_NE(nullptr, strstr(buf, "\\\"")) << "buffer=" << buf;

    CVector w;
    ASSERT_GT(w.load_buffer(buf), 0L) << "buffer=" << buf;
    EXPECT_EQ(w.count(), 3);
    EXPECT_STREQ(w[0], "a\"b");
    EXPECT_STREQ(w[1], "c\\d");
    EXPECT_STREQ(w[2], "e/f");
}

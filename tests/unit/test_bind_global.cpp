// test_bind_global.cpp — global / pure-function binds scattered across modules

// (util, core, combat, net, db, game managers, skill).

//

// Only binds that are safe on a bare lua_State (no g_LuaEngine / real script

// loaders) are exercised here; loader-style binds live in test_bind_loader.cpp.

#include "test_bind_common.h"

#include "User.h"

#include "UserMgr.h"  // find_player/remove_user（add_user 绑定的可观察面）

extern char g_notify_fail_msg[512];

#include "Message.h"        // chinese_number 直驱
#include "Attack.h"         // CAttack::damge_to_string（下放对拍基准）
#include "LoginRegister.h"  // check_legal_name（下放对拍基准）

#include "ServerConfig.h"

#include "test_state_recorder.h"  // 频道定向投递观察面

#include "test_char_common.h"  // CharMockDb

#include "HeartBeat.h"  // g_destruct_list 队列断言

#include "NpcMgr.h"

#include "Combatd.h"  // g_combatd 直调徒手包装器

#include "Item.h"  // 武器参数（CItem 即 CContainer）

#include <windows.h>  // LuaItem.inl 需 UUID/UuidCreate（前置解符号）

#include "LuaItem.h"  // CLuaEquip（l_do_action_base 分支）

#include "Npc.h"

#include "Sql.h"  // g_dataset_table_ddl

#include "mock/mock_database.h"  // SetShouldFail/SetQueryResultMatching

class BindGlobalTest : public BindTest {
protected:
    CChar ch;

    CContainer c;

    void SetUp() override

    {
        BindTest::SetUp();

        ch.set_name("g", "g1");

        c.set_name("box", "box1");
    }
};

// ---------------- util/Random.cpp ----------------

TEST_F(BindGlobalTest, RandomAndXrandom)

{
    lua_pushinteger(L, 10);

    int64_t v = CallInt(l_random, 1);

    EXPECT_GE(v, 0);

    EXPECT_LT(v, 10);

    lua_pushinteger(L, 100);

    EXPECT_GE(CallInt(l_xrandom, 1), 0);
}

// ---------------- util/StringUtil.cpp ----------------

TEST_F(BindGlobalTest, ValidString)

{
    // 契约：合法文本返回 1；含引号文本经 notify_fail 写入失败缓冲并返回 0。
    // 下放后由内容层前导实现，行为与原 C++ 绑定逐字一致。
    ASSERT_NE(ensure_lua_env(), nullptr);
    lua_State* E = ensure_lua_env();

    EXPECT_EQ(1, lua_int_expr(E, "valid_string('hello')"));
    EXPECT_EQ(1, lua_int_expr(E, "valid_string('')"));
    EXPECT_EQ(0, lua_int_expr(E, "valid_string('a\"b')"));
    EXPECT_STREQ("不能含有引号!", g_notify_fail_msg);
}

// ---------------- util/Logger.cpp ----------------

TEST_F(BindGlobalTest, LogLevelBindings)

{
    // log(level, fmt, ...) 桥接：无返回值；fmt 缺席经 luaL_checkstring
    // 报错、未登记级别名经 luaL_error 报错（call() 捕获为 -1）

    lua_pushstring(L, "info");

    lua_pushstring(L, "debug msg");

    EXPECT_EQ(0, CallRetCount(l_log, 2));

    // 全部六个级别名均可分派（trace/fatal 与中间四级同桥）

    lua_pushstring(L, "trace");

    lua_pushstring(L, "trace msg");

    EXPECT_EQ(0, CallRetCount(l_log, 2));

    lua_pushstring(L, "fatal");

    lua_pushstring(L, "fatal msg");

    EXPECT_EQ(0, CallRetCount(l_log, 2));

    lua_pushstring(L, "error");

    lua_pushstring(L, "err %d");

    lua_pushinteger(L, 5);

    EXPECT_EQ(0, CallRetCount(l_log, 3));  // 可变参数格式化路径

    // 缺 fmt → 报错

    lua_pushstring(L, "info");

    EXPECT_EQ(-1, call(l_log, 1));

    // 未登记的级别名 → 报错

    lua_pushstring(L, "bogus");

    lua_pushstring(L, "msg");

    EXPECT_EQ(-1, call(l_log, 2));
}

// ---------------- net/LoginRegister.cpp ----------------

TEST_F(BindGlobalTest, CheckLegalName)

{
    // 契约：合法名字返回 ""（绑定头注释），返回值是字符串而非 nil。
    // 下放后由内容层前导实现，与 C++ check_legal_name（长度检查 + 回环
    // login.check_name）同式同源。
    ASSERT_NE(ensure_lua_env(), nullptr);
    lua_State* E = ensure_lua_env();

    for (const char* name : {"zhangsan", "ab", "abcdefghij"}) {
        const std::string cpp = check_legal_name(name);
        const std::string lua = lua_str_expr(
            E, ("check_legal_name('" + std::string(name) + "')").c_str());
        EXPECT_EQ(cpp, lua) << name;
    }

    EXPECT_TRUE(lua_str_expr(E, "check_legal_name('zhangsan')").empty());
}

// ---------------- net/Message.cpp ----------------

TEST_F(BindGlobalTest, ChineseNumber)

{
    // 下放后由内容层前导实现，与 C++ chinese_number（全分支树）逐值对拍
    ASSERT_NE(ensure_lua_env(), nullptr);
    lua_State* E = ensure_lua_env();

    const signed long vals[]
        = {-5,   15,    45,    105,    115,       4000,      4050,
           4500, 40000, 40500, 450000, 400000000, 405000000, 450000000};

    for (signed long v : vals) {
        char buf[256] = {0};
        chinese_number(v, buf);

        EXPECT_EQ(std::string(buf),
                  lua_str_expr(
                      E, ("chinese_number(" + std::to_string(v) + ")").c_str()))
            << "v=" << v;
    }

    EXPECT_FALSE(lua_str_expr(E, "chinese_number(123)").empty());
}

TEST_F(BindGlobalTest, TellAndVisionGuards)

{
    // 消息类桥接（mock_net 接管）：void 无返回、pcall 不抛

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hi");

    EXPECT_EQ(0, CallRetCount(l_tell_object, 2));

    lua_pushstring(L, "vision");

    lua_pushstring(L, "look around");

    EXPECT_EQ(0, CallRetCount(l_message, 2));

    lua_pushstring(L, "room");

    lua_bind_push_object(L, &c);

    lua_pushstring(L, "room msg");

    EXPECT_EQ(0, CallRetCount(l_message, 3));

    // message kind="channel" with no me -> guard returns

    lua_pushstring(L, "channel");

    lua_pushstring(L, "chat");

    lua_pushstring(L, "hello");

    EXPECT_EQ(0, CallRetCount(l_message, 3));
}

// 未登记 KIND 是脚本写错，与 dbase/load 的未知 action 同律报错
TEST_F(BindGlobalTest, MessageUnknownKindErrors)
{
    lua_pushstring(L, "nope");
    EXPECT_EQ(-1, call(l_message, 1));
}

// ---------------- net/LoginAuth.cpp ----------------

TEST_F(BindGlobalTest, UserpWizardp)

{
    // 裸 CChar：非 IS_USER、非 wizard（谓词契约，非身份硬编码）

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "user");

    EXPECT_FALSE(CallBool(l_is, 2));

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "wizard");

    EXPECT_FALSE(CallBool(l_is, 2));

    // 授予巫师授权后谓词翻真

    ch.set("grant", (long)CChar::GRANT_WIZARD);

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "wizard");

    EXPECT_TRUE(CallBool(l_is, 2));

    // 未登记 KIND / 缺 KIND 报错（call() 捕获为 -1）

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "bogus");

    EXPECT_EQ(-1, call(l_is, 2));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(-1, call(l_is, 1));
}

// ---------------- net/Login.cpp ----------------

TEST_F(BindGlobalTest, IpAndOnline)

{
    lua_bind_push_object(L, &ch);

    std::string s;

    CallStr(l_get_ip, 1, s);

    lua_pushstring(L, "online");

    lua_pushstring(L, "nobody");

    int k = call(l_find, 2);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    k = call(l_find_online_users, 0);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_istable(L, -1));

    lua_pop(L, k);
}

// ---------------- cmd/CmdHandle.cpp ----------------

TEST_F(BindGlobalTest, NotifyFail)

{
    lua_pushstring(L, "you fail");

    EXPECT_EQ(0, CallInt(l_notify_fail, 1));
}

TEST_F(BindGlobalTest, GetRoomChar)

{
    // 非角色参数 → nil（守卫）

    lua_bind_push_object(L, &c);

    lua_pushinteger(L, 1);

    int k = call_global(L, "get_room_char", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    ch.move(&c);

    // target=0 → 不做房间查找 → nil

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 0);

    k = call_global(L, "get_room_char", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    // 命中同房间角色

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, ch.object_id());

    k = call_global(L, "get_room_char", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isuserdata(L, -1));

    lua_pop(L, k);

    // 命中非角色物件：present 有结果但 IS_CHAR 不成立 → nil

    CContainer box;

    box.set_name("box2", "box2");

    box.move(&c);

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, box.object_id());

    k = call_global(L, "get_room_char", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    // 房间里没有该编号 → nil

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 123456789);

    k = call_global(L, "get_room_char", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    box.move(nullptr);

    ch.move(nullptr);
}

TEST_F(BindGlobalTest, IsRelative)

{
    CChar other;

    other.set_name("o", "o1");

    // 非角色参数 → false（守卫），且必须回布尔而非数字
    lua_bind_push_object(L, &c);

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(1, call_global(L, "is_relative", 2));

    EXPECT_TRUE(lua_isboolean(L, -1));

    lua_pop(L, 1);

    // 同房间两人：无亲属关系 → false

    ch.move(&c);

    other.move(&c);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &other);

    int k = call_global(L, "is_relative", 2);

    ASSERT_EQ(1, k);

    EXPECT_FALSE(lua_toboolean(L, -1));

    lua_pop(L, k);

    // 一人处于另一人身体内（环境包含）→ true

    ch.move(&other);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &other);

    k = call_global(L, "is_relative", 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_toboolean(L, -1));

    lua_pop(L, k);

    ch.move(nullptr);

    other.move(nullptr);
}

// ---------------- object/Global.cpp ----------------

TEST_F(BindGlobalTest, TimeHelpers)

{
    EXPECT_GE(CallInt(l_current_time, 0), 0);

    // current_time_info 现由前导组合（os.date）：wday 口径 0=周日、hour 0-23
    int k = call_global(L, "current_time_info", 0);

    ASSERT_EQ(1, k);

    ASSERT_TRUE(lua_istable(L, -1));

    lua_getfield(L, -1, "wday");

    int wday = (int)lua_tointeger(L, -1);

    lua_pop(L, 1);

    EXPECT_GE(wday, 0);

    EXPECT_LE(wday, 6);

    lua_getfield(L, -1, "hour");

    int hour = (int)lua_tointeger(L, -1);

    lua_pop(L, 1);

    EXPECT_GE(hour, 0);

    EXPECT_LE(hour, 23);

    lua_pop(L, k);
}

// ---------------- engine/GameObject.cpp ----------------

TEST_F(BindGlobalTest, LivingFind)

{
    // is_living = query_attr(IS_LIVING)：普通容器不具备、角色具备

    lua_bind_push_object(L, &c);

    lua_pushstring(L, "living");

    EXPECT_FALSE(CallBool(l_is, 2));

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "living");

    EXPECT_TRUE(CallBool(l_is, 2));

    // character 谓词：角色为真、普通容器为假

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "character");

    EXPECT_TRUE(CallBool(l_is, 2));

    lua_bind_push_object(L, &c);

    lua_pushstring(L, "character");

    EXPECT_FALSE(CallBool(l_is, 2));
}

TEST_F(BindGlobalTest, RandomGetItemEmpty)

{
    lua_bind_push_object(L, &c);

    int k = call_global(L, "random_get_item", 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));  // empty container -> nil

    lua_pop(L, k);
}

// destruct with a non-userdata arg -> ob stays NULL, guard skips destruct()

TEST_F(BindGlobalTest, DestructNilGuard)

{
    lua_pushnil(L);

    EXPECT_EQ(0, CallRetCount(l_destruct, 1));
}

// ---------------- game managers: load/find (mock-backed / list-backed) ------

TEST_F(BindGlobalTest, ItemMgrLoadFind)

{
    CMockItemMgr mgr;

    // 换入前保存进程共享管理器（test_env 的 g_testItemMgr），退出时归还；

    // 置空会让下游隐式触达 g_item_mgr 的路径（如 find_object）踩空

    IItemMgr* saved_mgr = g_item_mgr;

    g_item_mgr = &mgr;

    lua_pushstring(L, "item");

    lua_pushstring(L, "hammer");

    int k = call(l_load, 2);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));  // mock returns nullptr

    lua_pop(L, k);

    lua_pushstring(L, "item");

    lua_pushinteger(L, 1);

    k = call(l_find, 2);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    lua_pushstring(L, "fb");

    lua_pushstring(L, "fb");

    k = call(l_load, 2);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    k = call(l_random_vendor, 0);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    g_item_mgr = saved_mgr;
}

TEST_F(BindGlobalTest, NpcUserListFind)

{
    lua_pushstring(L, "npc");

    lua_pushinteger(L, 1);

    int k = call(l_find, 2);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    k = call(l_random_npc, 0);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    lua_pushstring(L, "player");

    lua_pushinteger(L, 1);

    k = call(l_find, 2);

    EXPECT_EQ(1, k);

    lua_pop(L, k);
}

// ---------------- combat ----------------

// ---------------- user/UserMgr.cpp ----------------

TEST_F(BindGlobalTest, AddUserRegistersAndReturnsObject)

{
    extern MAP2USER g_user_list;  // add_user() 的落点

    MAP2USER before = g_user_list;  // 快照，用于识别新增项

    int k = call(l_add_user, 0);

    ASSERT_EQ(1, k) << "add_user() 应返回一个值(118-121)";

    ASSERT_TRUE(lua_isuserdata(L, -1)) << "返回用户对象而非 nil";

    auto* u = dynamic_cast<CUser*>(lua_bind_check_object(L, -1));

    ASSERT_NE(u, nullptr) << "栈上是可用的 CUser";

    lua_pop(L, k);

    // 可观察行为：新用户登记进用户表，且 find_player 按 oid 取回同一对象

    EXPECT_EQ(before.size() + 1, g_user_list.size()) << "新用户入表";

    EXPECT_EQ(u, g_user_list[u->object_id()]) << "按 oid 可取回同一对象";

    EXPECT_EQ(u, find_player(u->object_id())) << "find_player 命中该用户";

    // 收尾：从用户表摘除以复原全局状态；对象本体保留（lua 侧若存引用

    // 指向它，触碰的仍是活对象，避免 use-after-free）

    remove_user(u->object_id());

    EXPECT_EQ(before.size(), g_user_list.size()) << "用户表已复原";
}

TEST_F(BindGlobalTest, CombatAttackGuards)

{
    // 缺 target → me&&target 守卫跳过 Do_Attack（void 无返回且不抛）

    lua_pushstring(L, "normal");

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_combat_attack, 2));

    // unarmed 缺 skill（第 4 参）→ luaL_checkstring 抛错被 call() 捕获

    lua_pushstring(L, "unarmed");

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(-1, CallRetCount(l_combat_attack, 3));

    // unarmed 齐参但 target 非 userdata → me&&target 守卫跳过

    lua_pushstring(L, "unarmed");

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "no_target");

    lua_pushstring(L, "strike");

    EXPECT_EQ(0, CallRetCount(l_combat_attack, 4));

    // weapon 缺武器（第 4 参非 userdata）→ 守卫跳过

    lua_pushstring(L, "weapon");

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_combat_attack, 3));

    // 未登记 KIND 报错

    lua_pushstring(L, "oops");

    EXPECT_EQ(-1, CallRetCount(l_combat_attack, 1));

    // killer_reward: victim 缺失 → check_char 返回 NULL → 守卫跳过

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_killer_reward, 1));
}

TEST_F(BindGlobalTest, DamgeToString)

{
    // 下放后由内容层前导实现，与 CAttack::damge_to_string（伤害分档 + real
    // 占位）逐值对拍
    ASSERT_NE(ensure_lua_env(), nullptr);
    lua_State* E = ensure_lua_env();

    for (long long d :
         {0LL, 1LL, 1000LL, 1001LL, 5000LL, 5001LL, 10000LL, 10001LL, 1234LL}) {
        for (int real = 0; real <= 1; ++real) {
            const std::string cpp = CAttack::damge_to_string(d, real != 0);
            const std::string lua
                = lua_str_expr(E, ("damge_to_string(" + std::to_string(d) + ","
                                   + (real ? "true" : "false") + ")")
                                      .c_str());
            EXPECT_EQ(cpp, lua) << "d=" << d << " real=" << real;
        }
    }

    // 契约：伤害值 → 非空描述串
    EXPECT_FALSE(lua_str_expr(E, "damge_to_string(1234)").empty());
}

TEST_F(BindGlobalTest, CombatdQuery)

{
    // 契约：缺省键返回 0（CCombatConfig::query）

    lua_pushstring(L, "some_key");

    EXPECT_EQ(0, CallInt(l_combatd_query, 1));
}

// ---------------- db (needs mock g_db) ----------------

TEST_F(BindGlobalTest, DbQueryExecute)

{
    CharMockDb db;

    g_db = &db;

    lua_pushstring(L, "select 1");

    int k = call(l_db_query, 1);

    EXPECT_GE(k, 1);

    lua_pop(L, k);

    lua_pushstring(L, "update t set x=1");

    k = call(l_db_execute, 1);

    EXPECT_GE(k, 1);

    lua_pop(L, k);

    g_db = nullptr;
}

// 无库态契约：校验器这类无头进程没有 g_db，db_* 必须降级成脚本已有的失败
// 形态（false / nil + 错误串），不能空指针解引用。
TEST_F(BindGlobalTest, DbBindsDegradeWithoutDatabase)
{
    g_db = nullptr;

    lua_pushstring(L, "insert into t values(1)");
    int k = call(l_db_execute, 1);
    EXPECT_EQ(2, k) << "无库态 execute 返回 false + 错误串";
    if (k >= 2) {
        EXPECT_FALSE(lua_toboolean(L, -2));
        EXPECT_TRUE(lua_isstring(L, -1));
    }
    if (k > 0) lua_pop(L, k);

    lua_pushstring(L, "select 1");
    k = call(l_db_query, 1);
    EXPECT_EQ(2, k) << "无库态 query 返回 nil + 错误串";
    if (k >= 2) {
        EXPECT_TRUE(lua_isnil(L, -2));
        EXPECT_TRUE(lua_isstring(L, -1));
    }
    if (k > 0) lua_pop(L, k);
}

TEST_F(BindGlobalTest, SqlRankAndCharge)

{
    CharMockDb db;

    g_db = &db;

    lua_bind_push_object(L, &c);

    CallInt(l_paihangbang, 1);

    g_db = nullptr;
}

// ---------------- item/Money.cpp ----------------

TEST_F(BindGlobalTest, PayKind)

{
    CMockItemMgr mgr;

    ItemFactory f(&mgr);  // pay 经 load_item("coin") 发放，需工厂

    lua_pushstring(L, "give");

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 1);  // 单枚：amount>1 会触发 move() 的“涉嫌复制”守卫

    EXPECT_EQ(1, CallInt(l_pay_money, 3));

    // 发放的货币确实进入玩家背包（工厂 id="item"）

    EXPECT_NE(ch.present("item"), nullptr);

    // 背包没有 coin → 扣取失败返回 0（take 分支契约）

    lua_pushstring(L, "take");

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 50);

    EXPECT_EQ(0, CallInt(l_pay_money, 3));

    // 未登记 KIND 报错

    lua_pushstring(L, "nope");

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 1);

    EXPECT_EQ(-1, call(l_pay_money, 3));
}

// ---------------- engine/Channel.cpp ----------------

TEST_F(BindGlobalTest, DoChannel)

{
    // me = &g_Channel fallback (no userdata at 1), who = NULL, channel+msg

    // void 转发、pcall 不抛

    lua_pushnil(L);

    lua_pushnil(L);

    lua_pushstring(L, "chat");

    lua_pushstring(L, "hello world");

    EXPECT_EQ(0, CallRetCount(l_do_channel, 4));
}

// ---------------- CharFight / misc guards ----------------

TEST_F(BindGlobalTest, AddMagicGuard)

{
    // add_magic needs a CLuaSkill; plain CChar -> guard returns 0

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "n");

    lua_pushstring(L, "d");

    lua_pushinteger(L, 10);

    lua_pushinteger(L, 100);

    lua_pushinteger(L, 3);

    lua_pushinteger(L, 0);

    lua_pushstring(L, "f");

    EXPECT_EQ(1, CallRetCount(l_add_magic, 8));
}

// ---------------- db binds 的失败分支与数据行 ----------------

// （DbQueryExecute 只覆盖了成功空结果；此处补 错误回执/行构建/DDL 登记）

TEST_F(BindGlobalTest, DbExecute_ErrorReportsMessage)

{
    MockDatabase db;

    db.SetShouldFail(true);

    g_db = &db;

    lua_pushstring(L, "insert into t values(1)");

    int k = call(l_db_execute, 1);

    EXPECT_EQ(2, k) << "失败返回两个值：false + 错误串";

    if (k >= 2) {
        EXPECT_FALSE(lua_toboolean(L, -2));

        EXPECT_STREQ("mock execute failure", lua_tostring(L, -1));
    }

    lua_pop(L, k);

    g_db = nullptr;
}

TEST_F(BindGlobalTest, DbQuery_ErrorReportsNil)

{
    MockDatabase db;

    db.SetShouldFail(true);

    g_db = &db;

    lua_pushstring(L, "select * from t");

    int k = call(l_db_query, 1);

    EXPECT_EQ(2, k) << "失败返回两个值：nil + 错误串";

    if (k >= 2) {
        EXPECT_TRUE(lua_isnil(L, -2));

        EXPECT_STREQ("mock query failure", lua_tostring(L, -1));
    }

    lua_pop(L, k);

    g_db = nullptr;
}

TEST_F(BindGlobalTest, DbQuery_BuildsRowsTable)

{
    MockDatabase db;

    db.SetQueryResultMatching("select demo",

                              MockDatabase::MakeSingleRow({"alpha", "beta"}));

    g_db = &db;

    lua_pushstring(L, "select demo from t");

    int k = call(l_db_query, 1);

    EXPECT_EQ(1, k);

    if (k >= 1 && lua_istable(L, -1)) {
        EXPECT_EQ(1u, (unsigned)lua_rawlen(L, -1)) << "一行结果";

        lua_rawgeti(L, -1, 1);  // 第一行子表

        EXPECT_TRUE(lua_istable(L, -1));

        if (lua_istable(L, -1)) {
            EXPECT_EQ(2u, (unsigned)lua_rawlen(L, -1)) << "两列";

            lua_rawgeti(L, -1, 1);

            EXPECT_STREQ("alpha", lua_tostring(L, -1));

            lua_pop(L, 1);

            lua_rawgeti(L, -1, 2);

            EXPECT_STREQ("beta", lua_tostring(L, -1));

            lua_pop(L, 1);
        }

        lua_pop(L, 1);  // 第一行子表
    }

    lua_pop(L, k);

    g_db = nullptr;
}

// 行索引契约：结果集第 i 行落在 Lua 侧第 i+1 个键，多行时不得串位或塌缩。
TEST_F(BindGlobalTest, DbQuery_MultiRowKeepsRowPositions)
{
    MockDatabase db;

    DbResultSet rs;
    DbRow first;
    first.columns = {"r1c1", "r1c2"};
    DbRow second;
    second.columns = {"r2c1", "r2c2"};
    rs.rows.push_back(first);
    rs.rows.push_back(second);

    db.SetQueryResultMatching("select multi", rs);

    g_db = &db;

    lua_pushstring(L, "select multi from t");

    int k = call(l_db_query, 1);

    EXPECT_EQ(1, k);

    if (k >= 1 && lua_istable(L, -1)) {
        EXPECT_EQ(2u, (unsigned)lua_rawlen(L, -1)) << "两行";

        lua_rawgeti(L, -1, 1);  // 第一行子表

        if (lua_istable(L, -1)) {
            EXPECT_EQ(2u, (unsigned)lua_rawlen(L, -1)) << "第一行两列";

            lua_rawgeti(L, -1, 1);
            EXPECT_STREQ("r1c1", lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 2);
            EXPECT_STREQ("r1c2", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        lua_pop(L, 1);  // 第一行子表

        lua_rawgeti(L, -1, 2);  // 第二行子表

        if (lua_istable(L, -1)) {
            EXPECT_EQ(2u, (unsigned)lua_rawlen(L, -1)) << "第二行两列";

            lua_rawgeti(L, -1, 1);
            EXPECT_STREQ("r2c1", lua_tostring(L, -1));
            lua_pop(L, 1);

            lua_rawgeti(L, -1, 2);
            EXPECT_STREQ("r2c2", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        lua_pop(L, 1);  // 第二行子表
    }

    lua_pop(L, k);

    g_db = nullptr;
}

TEST_F(BindGlobalTest, DbRegisterDdl_RegistersOnce)

{
    const size_t before = g_dataset_table_ddl.size();

    const char* ddl = "CREATE TABLE t_bind_ddl_probe (id int)";

    lua_pushstring(L, ddl);

    EXPECT_EQ(0, call(l_db_register_ddl, 1));

    lua_pushstring(L, ddl);  // 热重载重复登记

    EXPECT_EQ(0, call(l_db_register_ddl, 1));

    int hits = 0;

    for (const std::string& s : g_dataset_table_ddl) {
        if (s == ddl) hits++;
    }

    EXPECT_EQ(1, hits) << "重复 DDL 只登记一次";

    EXPECT_EQ(before + 1, g_dataset_table_ddl.size());

    // 还原进程级容器，不污染 DbTables 用例

    g_dataset_table_ddl.resize(before);
}

// 回归：dataset 的 card_core.init_table 经全局 get_db_type() 选 SQLite/MySQL
// 两套 DDL；绑定丢失时登录期 login.on_login 直接抛
// "attempt to call a nil value (global 'get_db_type')"。
TEST_F(BindGlobalTest, GetDbTypeRegisteredAsGlobal)
{
    int top = lua_gettop(L);

    lua_getglobal(L, "get_db_type");
    ASSERT_TRUE(lua_isfunction(L, -1)) << "get_db_type 已注册为全局函数";

    ASSERT_EQ(LUA_OK, lua_pcall(L, 0, 1, 0));
    ASSERT_TRUE(lua_isstring(L, -1));

    // Lua 侧必须看到与 C++ 侧一致的库类型，脚本据此分支建表语句
    EXPECT_STREQ(get_db_type(), lua_tostring(L, -1));

    lua_settop(L, top);
}

// ---------------- net/Message.cpp：数字转中文的全量级分支 ----------------

TEST_F(BindGlobalTest, ChineseNumber_AllMagnitudes)

{
    // 覆盖 -5/十几/几十/几百（零位、十几余）/几千（整、零位、余）/几万（整、

    // 千位余、一般）/几亿（整、万位余、一般）全部分支树

    const signed long vals[]

        = {-5,   15,    45,    105,    115,       4000,      4050,

           4500, 40000, 40500, 450000, 400000000, 405000000, 450000000};

    for (signed long v : vals) {
        char buf[256] = {0};

        chinese_number(v, buf);

        EXPECT_FALSE(std::string(buf).empty()) << "v=" << v;
    }

    char b[256];

    chinese_number(-5, b);

    EXPECT_EQ(0u, std::string(b).find("负")) << "负号前缀(25)";

    chinese_number(15, b);

    EXPECT_NE(std::string::npos, std::string(b).find("十")) << "十几(32)";

    chinese_number(4500, b);

    EXPECT_NE(std::string::npos, std::string(b).find("千")) << "几千(61)";

    chinese_number(450000, b);

    EXPECT_NE(std::string::npos, std::string(b).find("万")) << "几万(72)";

    chinese_number(450000000, b);

    EXPECT_NE(std::string::npos, std::string(b).find("亿")) << "几亿(84)";
}

// ---------------- net/Message.cpp：定向频道消息的双人代词替换 ----------------

TEST_F(BindGlobalTest, MessageChannel_TargetedDelivery)

{
    CContainer* room = new CContainer();

    room->set("name", "chan_room");

    StateRecordingUser* me2 = new StateRecordingUser();

    me2->set_name("频道甲", "chan_me");

    me2->set("hp", (long)100);

    me2->move(room);

    me2->delete_temp("client/loading");

    StateRecordingUser* you2 = new StateRecordingUser();

    you2->set_name("频道乙", "chan_you");

    you2->set("hp", (long)100);

    you2->move(room);

    you2->delete_temp("client/loading");

    // kind="channel" + channel(str) + msg(str) + me(userdata) + you(userdata)

    lua_pushstring(L, "channel");

    lua_pushstring(L, "chat");

    lua_pushstring(L, "$N朝$n挥了挥手");

    lua_bind_push_object(L, me2);

    lua_bind_push_object(L, you2);

    EXPECT_EQ(0, CallRetCount(l_message, 5)) << "绑定全参(472-476)";

    EXPECT_TRUE(you2->has("频道甲")) << "对端代词替换+定向投递(155-164)";

    EXPECT_TRUE(me2->has("频道乙")) << "回执侧 $n 替换为对端名(166-169)";

    you2->move(nullptr);

    delete you2;

    me2->move(nullptr);

    delete me2;

    room->move(nullptr);

    delete room;
}

// ---------------- net/Message.cpp：message_vision / vision_room 绑定全参

// ----------------

TEST_F(BindGlobalTest, MessageVision_WithObjects_BindsFullArgs)

{
    CContainer* room = new CContainer();

    room->set("name", "vision_room");

    StateRecordingUser* actor = new StateRecordingUser();

    actor->set_name("施法者", "vis_me");

    actor->set("hp", (long)100);

    actor->move(room);

    actor->delete_temp("client/loading");

    // message(kind="vision", msg, me, you) → 绑定体取 userdata 深路

    lua_pushstring(L, "vision");

    lua_pushstring(L, "$N念了一句咒语");

    lua_bind_push_object(L, actor);

    lua_bind_push_object(L, actor);

    EXPECT_EQ(0, CallRetCount(l_message, 4)) << "绑定三参(443-447)";

    // kind="vision_room" + (me, channel, msg[, exclude1]) → 与排除位

    // 458-459

    lua_pushstring(L, "vision_room");

    lua_bind_push_object(L, actor);

    lua_pushstring(L, "chat");

    lua_pushstring(L, "咒语回声");

    lua_bind_push_object(L, actor);  // exclude1 → userdata 取值行(458-459)

    EXPECT_EQ(0, CallRetCount(l_message, 5))

        << "绑定四参(453-462)";

    EXPECT_FALSE(actor->has("咒语回声"))

        << "排除位生效：exclude1 自己不收(458-459)";

    actor->move(nullptr);

    delete actor;

    room->move(nullptr);

    delete room;
}

// ---------------- net/Message.cpp：私语绑定（self=true 深路） ----------------

TEST_F(BindGlobalTest, MessageVisionSelf_BindsFullArgs)

{
    CContainer* room = new CContainer();

    room->set("name", "self_room");

    StateRecordingUser* me2 = new StateRecordingUser();

    me2->set_name("密语者", "self_me");

    me2->set("hp", (long)100);

    me2->move(room);

    me2->delete_temp("client/loading");

    StateRecordingUser* you2 = new StateRecordingUser();

    you2->set_name("密友", "self_you");

    you2->set("hp", (long)100);

    you2->move(room);

    you2->delete_temp("client/loading");

    // kind="vision_self" + (msg, me, you) → 绑定体深路

    lua_pushstring(L, "vision_self");

    lua_pushstring(L, "$N低声对$n耳语");

    lua_bind_push_object(L, me2);

    lua_bind_push_object(L, you2);

    EXPECT_EQ(0, CallRetCount(l_message, 4)) << "绑定三参(439-447)";

    EXPECT_TRUE(you2->has("密语者")) << "私语送达对端并替换 $N(445)";

    you2->move(nullptr);

    delete you2;

    me2->move(nullptr);

    delete me2;

    room->move(nullptr);

    delete room;
}

// ---------------- cmd/CmdHandle.cpp：notify_fail 可变参组合 ----------------

TEST_F(BindGlobalTest, NotifyFail_FormatArgs_ComposesMessage)

{
    g_notify_fail_msg[0] = '\0';

    lua_pushstring(L, "你好%s，今日第%d次。");

    lua_pushstring(L, "大侠");

    lua_pushinteger(L, 7);

    EXPECT_EQ(0, CallInt(l_notify_fail, 3));

    EXPECT_STREQ(g_notify_fail_msg, "你好大侠，今日第7次。")

        << "string.format 组合后入 notify_fail(372-380)";
}

// ---------------- engine/GameObject.cpp（random_get_item / destruct /

//                  find_object） ----------------

TEST_F(BindGlobalTest, RandomGetItemDeepPath)

{
    // 51 件（末件作 exclude）：50 件上限 break + exclude 跳过 + type 过滤

    std::vector<CItem*> items;

    for (int i = 0; i < 51; i++) {
        auto* it = new CItem();

        char key[16];

        snprintf(key, 16, "gi%d", i);

        it->set("id", key);

        it->move(&ch);

        items.push_back(it);
    }

    CContainer* excl = items.back();

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, CMainObject::IS_ITEM);

    lua_bind_push_object(L, excl);

    int k = call_global(L, "random_get_item", 3);

    ASSERT_EQ(1, k);

    ASSERT_TRUE(lua_isuserdata(L, -1)) << "命中物件";

    CMainObject* got = lua_bind_check_object(L, -1);

    EXPECT_NE(got, excl) << "exclude 被跳过";

    EXPECT_TRUE(got->query_attr(CMainObject::IS_ITEM)) << "type 过滤";

    lua_pop(L, k);

    for (auto* it : items) {
        it->move(nullptr);

        delete it;
    }
}

TEST_F(BindGlobalTest, RandomGetItemNullArgsAndLivingFalse)

{
    lua_pushinteger(L, 1);  // 非 userdata → nil 守卫

    int k1 = call_global(L, "random_get_item", 1);

    ASSERT_EQ(1, k1);

    EXPECT_TRUE(lua_isnil(L, -1)) << "非 userdata → nil(205)";

    if (k1 > 0) lua_pop(L, k1);

    lua_pushinteger(L, 5);  // 非 userdata → 非对象

    lua_pushstring(L, "living");

    int k2 = call(l_is, 2);

    ASSERT_EQ(1, k2);

    EXPECT_FALSE(lua_toboolean(L, -1)) << "非对象 → false";

    if (k2 > 0) lua_pop(L, k2);
}

TEST_F(BindGlobalTest, DestructBindQueuesObject)

{
    auto* junk = new CItem();

    junk->set("id", "dstr_junk");

    lua_bind_push_object(L, junk);

    int k = call(l_destruct, 1);

    EXPECT_EQ(0, k) << "destruct 无返回值(233)";

    bool queued = false;

    {
        std::lock_guard<std::mutex> lk(g_destruct_list_mutex);

        for (auto* p : g_destruct_list) {
            if (p == junk) queued = true;
        }

        g_destruct_list.remove(junk);
    }

    EXPECT_TRUE(queued) << "destruct 摘注册表并入延迟析构队列(233)";

    delete junk;
}

TEST_F(BindGlobalTest, FindObjectBindReadsIdAndFlag)

{
    auto* npc = new CNpc();

    npc->set_name("路人", "fo_npc");

    g_npc_list[npc->object_id()] = npc;

    lua_pushstring(L, "object");

    lua_pushinteger(L, npc->object_id());

    lua_pushinteger(L, 1);  // flag=1: 只找 NPC/角色

    int k = call(l_find, 3);

    ASSERT_EQ(1, k) << "回推一个结果(243-244)";

    if (k >= 1) {
        CMainObject* got = lua_bind_check_object(L, -1);

        EXPECT_EQ(static_cast<CMainObject*>(npc), got)

            << "按 id 命中注册对象(241-243)";

        lua_pop(L, k);
    }

    g_npc_list.erase(npc->object_id());

    // 不 delete：l_find 已把它绑定进 lua 引用表
}

// 未登记 KIND → 报错（call() 捕获为 -1）

TEST_F(BindGlobalTest, FindUnknownKind)

{
    lua_pushstring(L, "bogus");

    lua_pushinteger(L, 1);

    EXPECT_EQ(-1, call(l_find, 2));
}

// ---------------- object/ContainerMove.cpp: l_environment ----------------

TEST_F(BindGlobalTest, EnvironmentBindReturnsContainerEnv)

{
    CContainer box;

    box.set_name("环境盒", "envbox1");

    // 无环境 → push nil(312)

    lua_bind_push_object(L, &box);

    int k = call(l_environment, 1);

    ASSERT_EQ(1, k) << "返回一个值";

    EXPECT_TRUE(lua_isnil(L, -1)) << "无环境 → nil";

    lua_pop(L, k);

    // 有环境 → push 环境对象(312)

    box.move(&c);

    lua_bind_push_object(L, &box);

    k = call(l_environment, 1);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isuserdata(L, -1)) << "返回环境对象(311-312)";

    auto* env = dynamic_cast<CContainer*>(lua_bind_check_object(L, -1));

    EXPECT_EQ(&c, env) << "环境即所在容器";

    lua_pop(L, k);

    box.move(nullptr);
}

// ---------------- combat/Combatd.cpp: 武器绑定 → 包装器 + 徒手包装器

// ----------------

TEST_F(BindGlobalTest, CombatAttackWeaponAndUnarmedWrappers)

{
    ASSERT_NE(ensure_lua_env(), nullptr) << "动作表/技能脚本走引擎";

    // 双方入房并具备战斗基本状态

    ch.move(&c);

    ch.set("hp", (long)100);

    ch.set("eff_hp", (long)100);

    ch.set("max_hp", (long)100);

    ch.set_skill("unarmed", 50);  // 动作表按技能解析

    CChar* foe = new CChar();

    foe->set_name("靶子", "cd_foe");

    foe->set("hp", (long)100);

    foe->set("eff_hp", (long)100);

    foe->set("max_hp", (long)100);

    foe->move(&c);

    CItem* weap = new CItem();

    weap->set_name("木棒", "cd_club");

    // combat_attack("weapon") 绑定：武器参数路径(123,126) →
    // do_attack_weapon 包装器(30-32)

    lua_pushstring(L, "weapon");

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, foe);

    lua_bind_push_object(L, weap);

    EXPECT_EQ(0, CallRetCount(l_combat_attack, 4))

        << "武器攻击绑定派发(123,126) 且经包装器(30-32)";

    EXPECT_EQ(1, foe->query_attr(CChar::IS_LIVING))

        << "武器绑定端到端执行后目标基本状态完好(123,126→30-32)";

    // 徒手技能包装器直调(36-38)

    g_combatd.do_attack_unarmed(&ch, foe, "unarmed", CCombatd::TYPE_QUICK);

    EXPECT_EQ(1, ch.query_attr(CChar::IS_LIVING))

        << "徒手包装器执行后攻击方状态完好(36-38)";

    // 收尾：清战斗/心跳态，摘除并释放临时对象

    ch.set_heart_beat(0);

    foe->set_heart_beat(0);

    ch.move(nullptr);

    foe->move(nullptr);

    delete foe;

    weap->move(nullptr);

    delete weap;
}

// ---------------- item/Item.cpp: l_do_action_base ----------------

TEST_F(BindGlobalTest, DoActionBaseBind)

{
    // do_action_base：CLuaEquip 分支

    ASSERT_NE(ensure_lua_env(), nullptr);  // CLuaEquip 构造走脚本环境

    CLuaEquip eq("use_eq_sword");

    lua_bind_push_object(L, &eq);

    lua_pushinteger(L, 0);  // act

    int k = call(l_do_action_base, 2);

    EXPECT_EQ(1, k) << "CLuaEquip 分支返回";

    lua_pop(L, k);
}

// ---------------- util/Random.cpp: l_xrandom 全档位采样(47,49,51,55)

// ----------------

TEST_F(BindGlobalTest, XrandomCoversAllWeightBands)

{
    // c = random(20) 分五档：<1 / <3 / <6 / <12 / else —— 400 次采样

    // 以压倒性概率命中全部档位（单档未中概率 ≤ (19/20)^400 ≈ 1.2e-9）

    int64_t lo = 1000000000, hi = -1;

    int iterations = 0;

    for (int i = 0; i < 400; i++) {
        lua_pushinteger(L, 120);

        int64_t v = CallInt(l_xrandom, 1);

        if (v < lo) lo = v;

        if (v > hi) hi = v;

        iterations++;
    }

    EXPECT_EQ(400, iterations) << "全部采样完成";

    EXPECT_GE(lo, 0) << "结果非负（档位输出不越下界）";

    EXPECT_LT(hi, 200) << "结果封顶 ≤ v*4/5 + v/5（档位输出不越上界）";
}

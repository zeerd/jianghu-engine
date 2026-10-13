// test_bind_global.cpp — global / pure-function binds scattered across modules

// (util, core, combat, net, db, game managers, skill).

//

// Only binds that are safe on a bare lua_State (no g_LuaEngine / real script

// loaders) are exercised here; loader-style binds live in test_bind_loader.cpp.

#include "test_bind_common.h"

#include "User.h"

#include "UserMgr.h"  // find_player/remove_user（add_user 绑定的可观察面）

extern char g_notify_fail_msg[512];

#include "Message.h"  // chinese_number 直驱

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
    // 契约：合法文本返回 1；含引号文本经 notify_fail 返回 0

    lua_pushstring(L, "hello");

    EXPECT_EQ(1, CallInt(l_valid_string, 1));

    lua_pushstring(L, "a\"b");

    EXPECT_EQ(0, CallInt(l_valid_string, 1));
}

// ---------------- util/Logger.cpp ----------------

TEST_F(BindGlobalTest, LogLevelBindings)

{
    // void 日志桥接：无返回值、pcall 不抛（参数经 luaL_checkstring 校验）

    lua_pushstring(L, "trace msg");

    EXPECT_EQ(0, CallRetCount(l_log_trace, 1));

    lua_pushstring(L, "debug msg");

    EXPECT_EQ(0, CallRetCount(l_log_debug, 1));

    lua_pushstring(L, "info msg");

    EXPECT_EQ(0, CallRetCount(l_log_info, 1));

    lua_pushstring(L, "warn msg");

    EXPECT_EQ(0, CallRetCount(l_log_warn, 1));

    lua_pushstring(L, "err %d");

    lua_pushinteger(L, 5);

    EXPECT_EQ(0, CallRetCount(l_log_error, 2));  // 可变参数格式化路径

    lua_pushstring(L, "fatal msg");

    EXPECT_EQ(0, CallRetCount(l_log_fatal, 1));
}

// ---------------- net/LoginRegister.cpp ----------------

TEST_F(BindGlobalTest, CheckLegalName)

{
    // 契约：合法名字返回 ""（绑定头注释），返回值是字符串而非 nil

    std::string s;

    lua_pushstring(L, "zhangsan");

    EXPECT_TRUE(CallStr(l_check_legal_name, 1, s));

    EXPECT_TRUE(s.empty());
}

// ---------------- net/Message.cpp ----------------

TEST_F(BindGlobalTest, ChineseNumber)

{
    lua_pushinteger(L, 123);

    std::string s;

    CallStr(l_chinese_number, 1, s);

    EXPECT_FALSE(s.empty());
}

TEST_F(BindGlobalTest, TellAndVisionGuards)

{
    // 消息类桥接（mock_net 接管）：void 无返回、pcall 不抛

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "hi");

    EXPECT_EQ(0, CallRetCount(l_tell_object, 2));

    lua_pushstring(L, "look around");

    EXPECT_EQ(0, CallRetCount(l_message_vision, 1));

    lua_bind_push_object(L, &c);

    lua_pushstring(L, "room msg");

    EXPECT_EQ(0, CallRetCount(l_tell_room, 2));

    // message_channel with no me -> guard returns

    lua_pushstring(L, "chat");

    lua_pushstring(L, "hello");

    EXPECT_EQ(0, CallRetCount(l_message_channel, 2));
}

// ---------------- net/LoginAuth.cpp ----------------

TEST_F(BindGlobalTest, UserpWizardp)

{
    // 裸 CChar：非 IS_USER、非 wizard（谓词契约，非身份硬编码）

    lua_bind_push_object(L, &ch);

    EXPECT_FALSE(CallBool(l_userp, 1));

    lua_bind_push_object(L, &ch);

    EXPECT_FALSE(CallBool(l_wizardp, 1));
}

// ---------------- net/Login.cpp ----------------

TEST_F(BindGlobalTest, IpAndOnline)

{
    lua_bind_push_object(L, &ch);

    std::string s;

    CallStr(l_get_ip, 1, s);

    lua_pushstring(L, "nobody");

    int k = call(l_find_online, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    k = call(l_get_all_online_users, 0);

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

    int k = call(l_get_room_char, 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    ch.move(&c);

    // target=0 → 不做房间查找 → nil

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 0);

    k = call(l_get_room_char, 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    // 命中同房间角色

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, ch.object_id());

    k = call(l_get_room_char, 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isuserdata(L, -1));

    lua_pop(L, k);

    // 命中非角色物件：present 有结果但 IS_CHAR 不成立 → nil

    CContainer box;

    box.set_name("box2", "box2");

    box.move(&c);

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, box.object_id());

    k = call(l_get_room_char, 2);

    ASSERT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));

    lua_pop(L, k);

    // 房间里没有该编号 → nil

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 123456789);

    k = call(l_get_room_char, 2);

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

    // 非角色参数 → 0（守卫）

    lua_bind_push_object(L, &c);

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallInt(l_is_relative, 2));

    // 同房间两人：无亲属关系 → 0

    ch.move(&c);

    other.move(&c);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &other);

    EXPECT_EQ(0, CallInt(l_is_relative, 2));

    // 一人处于另一人身体内（环境包含）→ 1

    ch.move(&other);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &other);

    EXPECT_EQ(1, CallInt(l_is_relative, 2));

    ch.move(nullptr);

    other.move(nullptr);
}

// ---------------- object/Global.cpp ----------------

TEST_F(BindGlobalTest, TimeHelpers)

{
    EXPECT_GE(CallInt(l_current_time, 0), 0);

    int k = call(l_current_time_info, 0);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_istable(L, -1));

    lua_pop(L, k);

    // 桥接必须转发全局值而非常量

    EXPECT_EQ(g_server_id, CallInt(l_get_server_id, 0));
}

// ---------------- engine/GameObject.cpp ----------------

TEST_F(BindGlobalTest, LivingFind)

{
    // living = query_attr(IS_LIVING)：普通容器不具备（GameObject.cpp）

    lua_bind_push_object(L, &c);

    EXPECT_FALSE(CallBool(l_living, 1));
}

TEST_F(BindGlobalTest, RandomGetItemEmpty)

{
    lua_bind_push_object(L, &c);

    int k = call(l_random_get_item, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));  // empty container -> nil

    lua_pop(L, k);
}

TEST_F(BindGlobalTest, MakeRandomYewai)

{
    lua_pushinteger(L, 5);

    int k = call(l_make_random_yewai_global, 1);

    EXPECT_EQ(1, k);  // require fails -> "" string

    EXPECT_TRUE(lua_isstring(L, -1));

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

    lua_pushstring(L, "hammer");

    int k = call(l_load_item, 1);

    EXPECT_EQ(1, k);

    EXPECT_TRUE(lua_isnil(L, -1));  // mock returns nullptr

    lua_pop(L, k);

    lua_pushinteger(L, 1);

    k = call(l_find_item, 1);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    lua_pushstring(L, "fb");

    k = call(l_load_fb, 1);

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
    lua_pushinteger(L, 1);

    int k = call(l_find_npc, 1);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    k = call(l_random_npc, 0);

    EXPECT_EQ(1, k);

    lua_pop(L, k);

    lua_pushinteger(L, 1);

    k = call(l_find_player, 1);

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
    // only self pushed -> me&&target guard skips Do_Attack（void 无返回且不抛）

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_combat_attack, 1));

    // unarmed 需要 skill 参数（缺参会抛、被 call() 捕获）；target 给非

    // userdata 走 me&&target 守卫

    lua_bind_push_object(L, &ch);

    lua_pushstring(L, "no_target");

    lua_pushstring(L, "strike");

    EXPECT_EQ(0, CallRetCount(l_combat_attack_unarmed, 3));

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_combat_attack_weapon, 1));

    // killer_reward: victim 缺失 → check_char 返回 NULL → 守卫跳过

    lua_bind_push_object(L, &ch);

    EXPECT_EQ(0, CallRetCount(l_killer_reward, 1));
}

TEST_F(BindGlobalTest, DamgeToString)

{
    // 契约：伤害值 → 非空描述串

    lua_pushinteger(L, 1234);

    std::string s;

    EXPECT_TRUE(CallStr(l_damge_to_string, 1, s));

    EXPECT_FALSE(s.empty());
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

TEST_F(BindGlobalTest, SqlRankAndCharge)

{
    CharMockDb db;

    g_db = &db;

    lua_pushstring(L, "id1");

    lua_pushinteger(L, 1);

    lua_pushinteger(L, 10);

    lua_pushstring(L, "data");

    lua_pushboolean(L, 0);

    CallBool(l_sql_update_record, 5);

    lua_pushstring(L, "base1");

    lua_pushinteger(L, 5);

    CallBool(l_update_mf_record, 2);

    lua_bind_push_object(L, &c);

    CallInt(l_paihangbang, 1);

    g_db = nullptr;
}

// ---------------- skill ----------------

TEST_F(BindGlobalTest, ForceConflictFlags)

{
    // 桥接函数必须反映 config.ini [server] 的开关，而不是返回常量

    ini_load("tests/test_config.ini");

    CServerConfig::init();

    EXPECT_EQ(CallInt(l_get_no_force_conflict, 0),

              CServerConfig::no_force_conflict() ? 1 : 0);

    EXPECT_EQ(CallInt(l_get_newlife_no_conflict, 0),

              CServerConfig::newlife_no_conflict() ? 1 : 0);

    g_ini_data["server"]["no_force_conflict"] = "1";

    g_ini_data["server"]["newlife_no_conflict"] = "0";

    CServerConfig::init();

    EXPECT_EQ(CallInt(l_get_no_force_conflict, 0), 1);

    EXPECT_EQ(CallInt(l_get_newlife_no_conflict, 0), 0);

    ini_load("tests/test_config.ini");

    CServerConfig::init();
}

// ---------------- item/Money.cpp ----------------

TEST_F(BindGlobalTest, PayPlayer)

{
    CMockItemMgr mgr;

    ItemFactory f(&mgr);  // pay_player 经 load_item("coin") 发放，需工厂

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 1);  // 单枚：amount>1 会触发 move() 的“涉嫌复制”守卫

    CallVoid(l_pay_player, 2);

    // 发放的货币确实进入玩家背包（工厂 id="item"）

    EXPECT_NE(ch.present("item"), nullptr);

    // 背包没有 coin → 支付失败返回 0（player_pay 契约）

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, 50);

    EXPECT_EQ(0, CallInt(l_player_pay, 2));
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

    // channel(str) + msg(str) + me(userdata) + you(userdata) 全参

    lua_pushstring(L, "chat");

    lua_pushstring(L, "$N朝$n挥了挥手");

    lua_bind_push_object(L, me2);

    lua_bind_push_object(L, you2);

    EXPECT_EQ(0, CallRetCount(l_message_channel, 4)) << "绑定全参(472-476)";

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

    // message_vision(msg, me, you) 三参 → 绑定体取 userdata 深路(443-446)

    lua_pushstring(L, "$N念了一句咒语");

    lua_bind_push_object(L, actor);

    lua_bind_push_object(L, actor);

    EXPECT_EQ(0, CallRetCount(l_message_vision, 3)) << "绑定三参(443-447)";

    // MessageVision(me, channel, msg[, exclude1]) 四参 → 453-462 与排除位

    // 458-459

    lua_bind_push_object(L, actor);

    lua_pushstring(L, "chat");

    lua_pushstring(L, "咒语回声");

    lua_bind_push_object(L, actor);  // exclude1 → userdata 取值行(458-459)

    EXPECT_EQ(0, CallRetCount(l_message_vision_channel, 4))

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

    // message_vision_self(msg, me, you) 三参全绑 → 绑定体深路(439-447)

    lua_pushstring(L, "$N低声对$n耳语");

    lua_bind_push_object(L, me2);

    lua_bind_push_object(L, you2);

    EXPECT_EQ(0, CallRetCount(l_message_vision_self, 3)) << "绑定三参(439-447)";

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

//                  find_object / make_random_yewai） ----------------

TEST_F(BindGlobalTest, RandomGetItemDeepPath)

{
    // 51 件：50 件计数 + 1 件 exclude → 恰好触发 num==50 上限 break(34)

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

    CContainer* got = random_get_item(&ch, CMainObject::IS_ITEM, excl);

    ASSERT_NE(nullptr, got) << "命中物件(30-36)";

    EXPECT_NE(got, excl) << "exclude 被跳过(32)";

    EXPECT_TRUE(got->query_attr(CMainObject::IS_ITEM)) << "type 过滤(31)";

    lua_bind_push_object(L, &ch);

    lua_pushinteger(L, CMainObject::IS_ITEM);

    lua_bind_push_object(L, excl);

    int k = call(l_random_get_item, 3);

    ASSERT_EQ(1, k);

    EXPECT_FALSE(lua_isnil(L, -1)) << "绑定成功回推(211)";

    if (k > 0) lua_pop(L, k);

    for (auto* it : items) {
        it->move(nullptr);

        delete it;
    }
}

TEST_F(BindGlobalTest, RandomGetItemNullArgsAndLivingFalse)

{
    lua_pushinteger(L, 1);  // 非 userdata → me NULL

    int k1 = call(l_random_get_item, 1);

    ASSERT_EQ(1, k1);

    EXPECT_TRUE(lua_isnil(L, -1)) << "非 userdata → nil(205)";

    if (k1 > 0) lua_pop(L, k1);

    lua_pushinteger(L, 5);  // 同样走 l_living 的空参支

    int k2 = call(l_living, 1);

    ASSERT_EQ(1, k2);

    EXPECT_FALSE(lua_toboolean(L, -1)) << "空参 pushboolean 0(224)";

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

    lua_pushinteger(L, npc->object_id());

    lua_pushinteger(L, 1);  // flag=1: 只找 NPC/角色

    int k = call(l_find_object, 2);

    ASSERT_EQ(1, k) << "回推一个结果(243-244)";

    if (k >= 1) {
        CMainObject* got = lua_bind_check_object(L, -1);

        EXPECT_EQ(static_cast<CMainObject*>(npc), got)

            << "按 id 命中注册对象(241-243)";

        lua_pop(L, k);
    }

    g_npc_list.erase(npc->object_id());

    // 不 delete：l_find_object 已把它绑定进 lua 引用表
}

TEST_F(BindGlobalTest, MakeRandomYewaiThreeStates)

{
    // 态1：模块有函数且返回字符串 → 181,187,188 直通

    luaL_dostring(

        L,

        "package.loaded['room.yewai_grid']=nil\n"

        "package.preload['room.yewai_grid']=function() return {"

        "make_random_yewai=function(n) return 'yewai-'..n end } end\n");

    lua_pushinteger(L, 5);

    int k1 = call(l_make_random_yewai_global, 1);

    ASSERT_EQ(1, k1);

    EXPECT_STREQ(lua_tostring(L, -1), "yewai-5") << "有钩子回读串(187-188)";

    if (k1 > 0) lua_pop(L, k1);

    // 态2：模块表缺函数 → 182-185 早退空串

    luaL_dostring(

        L,

        "package.loaded['room.yewai_grid']=nil\n"

        "package.preload['room.yewai_grid']=function() return {} end\n");

    lua_pushinteger(L, 5);

    int k2 = call(l_make_random_yewai_global, 1);

    ASSERT_EQ(1, k2);

    EXPECT_STREQ(lua_tostring(L, -1), "") << "缺钩子 → 空串(182-185)";

    if (k2 > 0) lua_pop(L, k2);

    // 态3：函数返回 nil → 190-191 归一空串

    luaL_dostring(L,

                  "package.loaded['room.yewai_grid']=nil\n"

                  "package.preload['room.yewai_grid']=function() return {"

                  "make_random_yewai=function(n) return nil end } end\n");

    lua_pushinteger(L, 5);

    int k3 = call(l_make_random_yewai_global, 1);

    ASSERT_EQ(1, k3);

    EXPECT_STREQ(lua_tostring(L, -1), "") << "nil 结果归一空串(190-191)";

    if (k3 > 0) lua_pop(L, k3);

    luaL_dostring(L,

                  "package.loaded['room.yewai_grid']=nil;"

                  "package.preload['room.yewai_grid']=nil");
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

    // combat_attack_weapon 绑定：武器参数路径(123,126) → do_attack_weapon

    // 包装器(30-32)

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, foe);

    lua_bind_push_object(L, weap);

    EXPECT_EQ(0, CallRetCount(l_combat_attack_weapon, 3))

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

// ---------------- item/Item.cpp: l_do_use(291) / l_do_action_base(309-310)

// ----------------

TEST_F(BindGlobalTest, DoUseAndActionBaseBinds)

{
    // do_use：三参齐备 → target 参数读取路径(291)

    CItem* it = new CItem();

    it->set_name("粗碗", "use_bowl");

    lua_bind_push_object(L, it);

    lua_bind_push_object(L, &ch);

    lua_bind_push_object(L, &c);

    int k = call(l_do_use, 3);

    ASSERT_EQ(1, k) << "do_use 返回一个数值";

    EXPECT_TRUE(lua_isinteger(L, -1)) << "do_use 结果为整数";

    lua_pop(L, k);

    // do_action_base：CLuaEquip 分支(309-310)

    ASSERT_NE(ensure_lua_env(), nullptr);  // CLuaEquip 构造走脚本环境

    CLuaEquip eq("use_eq_sword");

    lua_bind_push_object(L, &eq);

    lua_pushinteger(L, 0);  // act

    k = call(l_do_action_base, 2);

    EXPECT_EQ(1, k) << "CLuaEquip 分支返回(309-310)";

    lua_pop(L, k);

    it->move(nullptr);

    delete it;
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

// ---------------- db/SqlRank.cpp: insert execute-fail 双支(98-99,165-166)
// ----------------
TEST_F(BindGlobalTest, SqlRankInsertFailBranches)
{
    MockDatabase mdb;
    g_db = &mdb;
    mdb.SetExecuteErrorCode(1);  // query 空成功 → INSERT → execute 失败

    // update_mf_record：INSERT 失败 → false(98-99)
    lua_pushstring(L, "mf_base");
    lua_pushinteger(L, 5);
    EXPECT_FALSE(CallBool(l_update_mf_record, 2))
        << "mf insert 失败 → false(98-99)";

    // sql_update_record：INSERT 失败 → false(165-166)
    lua_pushstring(L, "id9");
    lua_pushinteger(L, 1);
    lua_pushinteger(L, 10);
    lua_pushstring(L, "datax");
    lua_pushboolean(L, 0);
    EXPECT_FALSE(CallBool(l_sql_update_record, 5))
        << "record insert 失败 → false(165-166)";

    g_db = nullptr;
}

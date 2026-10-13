// test_login_flow.cpp - 登录主流程（src/net/Login.cpp）的行为契约
//
// on_login 的入参是网关帧：[0..254] 为 XOR 编码的登录串
// （login <本地IP> <版本> <帐号> <密码> <卡号> <卡密>），[255..] 为对端
// IP 串。测试用镜像编码器构帧；serial/sockfd 一律传 0——网关发送函数
// （send_to_client/send_to_client_direct/open_client/close_client）把
// serial 转成 CGateInfo*，空指针直接跳过，天然无 socket 依赖。
//
// 帐号档案走 MockDatabase（cc_savedata/cc_freeze/cc_server 三条预置），
// verify_user 走 mock_db 的 g_mock_db_succeed 开关。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "Channel.h"
#include "GameSvr.h"     // CGateInfo / GATE_LOGIN_NORMAL
#include "GateClient.h"  // open_client
#include "GameInit.h"    // g_current_time / g_boot_time / g_server_id
#include "HeartBeat.h"   // g_destruct_list：DESTRUCT_OB 后回收自毁对象
#include "Item.h"
#include "Login.h"
#include "Mapping.h"
#include "Npc.h"
#include "Room.h"
#include "RoomMgr.h"
#include "StringUtil.h"  // decode_str（帧编解码与之互逆）
#include "User.h"
#include "lua/LuaBindDecls.h"
#include "lua/LuaEngine.h"
#include "mock/mock_database.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern bool g_mock_db_succeed;
extern MAP2USER g_user_list;
extern MAP2ROOM g_room_list;

// 生产函数无头文件声明（Login.cpp 内定义），测试按需直接调用
void down_bullet_info(CChar* me);

namespace {

constexpr int kNow      = 1700000000;
constexpr int kBootTime = kNow - 3600;

// 镜像 decode_str 的 XOR 编码器：空格/换行/制表符原样保留且同样推进
// 密钥游标。编码后经 decode_str 还原自检（BuildLoginFrame 内做）。
void encode_str(char* msg)
{
    int p = 0, t = 0;
    const char* key = "\15L\26A\18N\12W\25O\33O\6D";
    int len         = (int)strlen(key);
    while (msg[p]) {
        if (msg[p] != ' ' && msg[p] != '\n' && msg[p] != '\t') {
            msg[p] ^= key[t];
        }
        p++;
        t++;
        if (t >= len) t = 0;
    }
}

// 构造 on_login 帧：登录串编码进 [0..254]，IP 串放 [255..]。
// 返回 false 表示明文 XOR 后撞上保留字节（空格/制表符/换行/NUL），
// 需换用例参数——调用方须 ASSERT_TRUE。
bool BuildLoginFrame(char* buf, size_t cap, const char* plaintext,
                     const char* ip)
{
    memset(buf, 0, cap);
    if (strlen(plaintext) >= 254) return false;

    strcpy(buf, plaintext);
    char check[256];
    strcpy(check, buf);
    encode_str(check);
    decode_str(check);
    if (strcmp(check, plaintext) != 0) return false;  // 编码碰撞

    encode_str(buf);
    strncpy(&buf[255], ip, 79);
    return true;
}

// 明文格式：login <本地IP> <版本> <帐号> <密码> <卡号> <卡密>
const char* kLoginCmd = "login 8.8.8.8 1.0 10001 test1234 c1 p1";
// get_ip 的拼装结果：M:<对端IP> L:<本地IP>（各截 19 字符）
const char* kIpPair = "M:9.9.9.9 L:8.8.8.8";

}  // namespace

class LoginFlowTest : public ::testing::Test {
protected:
    MockDatabase* db        = nullptr;
    bool saved_mock_succeed = true;
    int saved_current_time  = 0;
    int saved_boot_time     = 0;
    // SetUp 时快照：TearDown 只回收本用例新增的对象
    std::vector<CContainer*> m_live_at_setup;
    std::vector<int32_t> m_live_users;
    std::vector<int32_t> m_live_npcs;
    std::vector<std::string> m_live_rooms;

    char frame[400];

    // 预置某帐号的档案行：restore_data 取顶层 "dbase" 子映射
    void PresetArchive(const char* id, const char* start_city,
                       const char* start_obj = "")
    {
        CMapping dbase;
        dbase.set("id", id);
        dbase.set("name", "测试者");
        dbase.set("gender", "男");
        dbase.set("str", 10L);
        dbase.set("con", 10L);
        dbase.set("int", 10L);
        dbase.set("dex", 10L);
        dbase.set("start_city", start_city);
        if (start_obj[0]) dbase.set("start_obj", start_obj);
        CMapping savedata;
        savedata.set("dbase", dbase);
        db->SetQueryResultMatching(
            "select cc_savedata from jianghudb",
            MockDatabase::MakeSingleRow({savedata.get_buffer()}));
    }

    // 构帧并断言成功（碰撞时给出原因）
    void MakeFrame(const char* cmd, const char* ip)
    {
        ASSERT_TRUE(BuildLoginFrame(frame, sizeof(frame), cmd, ip))
            << "登录串 XOR 编码碰撞，请更换用例参数: " << cmd;
    }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        saved_mock_succeed = g_mock_db_succeed;
        g_mock_db_succeed  = true;
        saved_current_time = g_current_time;
        g_current_time     = kNow;
        saved_boot_time    = g_boot_time;
        g_boot_time        = kBootTime;
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        for (auto& kv : g_user_list) m_live_users.push_back(kv.first);
        for (auto& kv : g_npc_list) m_live_npcs.push_back(kv.first);
        for (auto& kv : g_room_list) m_live_rooms.push_back(kv.first);

        db   = new MockDatabase();
        g_db = db;
        // 帐号未冻结且不跨服：cc_freeze/cc_server 无预置时 db 层按失败处理
        // （冻结/ -1 号服），必须显式给出放行值
        db->SetQueryResultMatching("select cc_freeze from jianghudb",
                                   MockDatabase::MakeSingleRow({"0"}));
        db->SetQueryResultMatching("select cc_server from jianghudb",
                                   MockDatabase::MakeSingleRow({"0"}));
        PresetArchive("10001", "login_start_room");
    }

    void TearDown() override
    {
        // 1) 本用例新增的在线用户：停心跳、脱房、从延迟列表摘除后删除。
        //    （成功登录的用户仍在 g_user_list；被拒路径的已自毁进延迟列表）
        std::vector<CUser*> new_users;
        for (auto& kv : g_user_list) {
            bool known = false;
            for (auto id : m_live_users) {
                if (id == kv.first) known = true;
            }
            if (!known) new_users.push_back(kv.second);
        }
        for (auto* u : new_users) {
            g_user_list.erase(u->object_id());
            u->set_heart_beat(0);
            u->move(nullptr);
            {
                std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
                g_destruct_list.remove(u);
            }
            delete u;
        }
        // 2) 延迟析构列表：只回收本用例新增的对象（被拒路径自毁的用户）
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {
                    ++it;
                    continue;
                }
                it = g_destruct_list.erase(it);
                delete ob;
            }
        }
        // 2.5) 本用例新增的 NPC（rk 的注册专员等）：摘心跳、脱房后删除，
        //      避免残留进 g_npc_list 影响后续用例（如 random_npc）
        {
            std::vector<CNpc*> new_npcs;
            for (auto& kv : g_npc_list) {
                bool known = false;
                for (auto id : m_live_npcs) {
                    if (id == kv.first) known = true;
                }
                if (!known) new_npcs.push_back(kv.second);
            }
            for (CNpc* n : new_npcs) {
                g_npc_list.erase(n->object_id());
                n->set_heart_beat(0);
                n->move(nullptr);
                delete n;
            }
        }
        // 3) 本用例 load_room 造出的房间（起始城/出生房等）
        std::vector<std::string> new_rooms;
        for (auto& kv : g_room_list) {
            bool known = false;
            for (auto& k : m_live_rooms) {
                if (k == kv.first) known = true;
            }
            if (!known) new_rooms.push_back(kv.first);
        }
        for (auto& key : new_rooms) {
            CRoom* r = g_room_list[key];
            g_room_list.erase(key);
            delete r;
        }
        // 4) 频道计数/封禁键复位
        g_channel.loginlist.del(kIpPair);
        g_channel.loginlimit.del(kIpPair);
        g_channel.banlist.del(kIpPair);
        // 5) 摘 db 与全局开关，恢复时钟
        g_db = nullptr;
        delete db;
        db                = nullptr;
        g_mock_db_succeed = saved_mock_succeed;
        g_current_time    = saved_current_time;
        g_boot_time       = saved_boot_time;
    }
};

// =====================================================================
// 帧编解码：镜像编码器必须与 decode_str 互逆
// =====================================================================
TEST_F(LoginFlowTest, LoginFrameCodec_RoundTrips)
{
    char buf[400];
    ASSERT_TRUE(BuildLoginFrame(buf, sizeof(buf), kLoginCmd, kIpPair));
    // decode_str 原地解码 [0..254] 段（遇 NUL 停止，不会越界到 IP 段）
    char cmd[256];
    strncpy(cmd, buf, 254);
    cmd[254] = 0;
    decode_str(cmd);
    EXPECT_STREQ(kLoginCmd, cmd);
    EXPECT_STREQ(kIpPair, &buf[255]);
}

// =====================================================================
// on_login 各拒绝分支（serial=0：网关发送为无操作）
// =====================================================================
TEST_F(LoginFlowTest, OnLogin_NotLoginCommand_Rejected)
{
    MakeFrame("who10001 test1234", "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    EXPECT_EQ(nullptr, find_online("10001")) << "非登录指令不得创建用户";
}

TEST_F(LoginFlowTest, OnLogin_BadPassword_Rejected)
{
    g_mock_db_succeed = false;  // verify_user 走 mock 开关
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
}

TEST_F(LoginFlowTest, OnLogin_FrozenAccount_Rejected)
{
    // 冻结值为 1：query_invalid_account 命中 → 拒绝
    db->Clear();
    db->SetQueryResultMatching("select cc_freeze from jianghudb",
                               MockDatabase::MakeSingleRow({"1"}));
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
}

TEST_F(LoginFlowTest, OnLogin_BannedIp_Rejected)
{
    g_channel.banlist.set(kIpPair, 1);
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    // 被拒用户已自毁：不得残留在在线表
    EXPECT_EQ(nullptr, find_online("10001"));
}

TEST_F(LoginFlowTest, OnLogin_LoginIpLimit_Rejected)
{
    // 该地址限 1 帐号且已有 1 个登录计数 → 新登录超限被拒
    g_channel.loginlimit.set(kIpPair, 1);
    g_channel.loginlist.add(kIpPair, 1);
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    EXPECT_EQ(nullptr, find_online("10001"));
}

TEST_F(LoginFlowTest, OnLogin_NoArchive_Rejected)
{
    // 清空预置：load_from_server 查不到档案 → 拒绝
    db->Clear();
    db->SetQueryResultMatching("select cc_freeze from jianghudb",
                               MockDatabase::MakeSingleRow({"0"}));
    db->SetQueryResultMatching("select cc_server from jianghudb",
                               MockDatabase::MakeSingleRow({"0"}));
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    EXPECT_EQ(nullptr, find_online("10001"));
}

TEST_F(LoginFlowTest, OnLogin_ServerMismatch_Rejected)
{
    // 档案在 99 号服：本进程不是 → 拒绝
    db->Clear();
    db->SetQueryResultMatching("select cc_freeze from jianghudb",
                               MockDatabase::MakeSingleRow({"0"}));
    db->SetQueryResultMatching("select cc_server from jianghudb",
                               MockDatabase::MakeSingleRow({"99"}));
    PresetArchive("10001", "login_start_room");
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    EXPECT_EQ(nullptr, find_online("10001"));
}

// =====================================================================
// on_login 成功登录：建档上线、出生房兜底、签到落库
// =====================================================================
TEST_F(LoginFlowTest, OnLogin_NewUser_LogsInAndFallsBackToStartRoom)
{
    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_TRUE(on_login(0, 0, frame));

    CUser* me = find_online("10001");
    ASSERT_NE(nullptr, me) << "登录成功后帐号应在线";
    // 起始城无存档（fail_ob）→ 回落到 g_start_room
    ASSERT_NE(nullptr, me->environment());
    EXPECT_STREQ(g_start_room, me->environment()->querystr("base_name"));
    // 登录痕迹落进 dbase
    EXPECT_STREQ(kIpPair, me->querystr("login/ip"));
    EXPECT_NE(0, me->query("login/time")) << "login/time 已写入";
    // 频道默认集已下发
    ASSERT_NE(nullptr, me->queryvec("channels"));
    EXPECT_EQ(6, me->queryvec("channels")->count());
}

TEST_F(LoginFlowTest, OnLogin_StartObj_OverridesStartCity)
{
    // 档案带 start_obj：优先加载该房，且用后即焚
    db->Clear();
    db->SetQueryResultMatching("select cc_freeze from jianghudb",
                               MockDatabase::MakeSingleRow({"0"}));
    db->SetQueryResultMatching("select cc_server from jianghudb",
                               MockDatabase::MakeSingleRow({"0"}));
    PresetArchive("10001", "login_start_room", "login_obj_room");

    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_TRUE(on_login(0, 0, frame));

    CUser* me = find_online("10001");
    ASSERT_NE(nullptr, me);
    ASSERT_NE(nullptr, me->environment());
    EXPECT_STREQ("login_obj_room", me->environment()->querystr("base_name"));
    EXPECT_EQ('\0', me->querystr("start_obj")[0]) << "start_obj 用后删除";
}

// =====================================================================
// 游客登录（guest/guest）：免查库、临时负数账号、进入游戏大厅 rk
// =====================================================================
namespace {

// 游客以负数临时账号在线，取本次登录后的计数器值反推账号
std::string CurrentGuestId()
{
    return std::string("-") + std::to_string(g_guest_account);
}

// rk 房间的专员 NPC 与房间本身不在 g_user_list 管理范围内，
// 用例结束时手动摘除（TearDown 只回收房间）
void CleanupRkRoomAndClerk()
{
    for (auto it = g_npc_list.begin(); it != g_npc_list.end();) {
        if (EQUALSTR(it->second->querystr("base_name"), "register_clerk")) {
            CNpc* npc = it->second;
            it        = g_npc_list.erase(it);
            npc->set_heart_beat(0);
            npc->move(nullptr);
            delete npc;
        }
        else {
            ++it;
        }
    }

    auto room = g_room_list.find("rk");
    if (room != g_room_list.end()) {
        CRoom* rk = room->second;
        // 先把还站在 rk 里的用户请出去：房间一删，环境指针就悬空
        for (auto& kv : g_user_list) {
            if (kv.second->environment() == rk) kv.second->move(nullptr);
        }
        g_room_list.erase(room);
        delete rk;
    }
}

}  // namespace

TEST_F(LoginFlowTest, IsGuestLogin_RequiresExactGuestGuest)
{
    EXPECT_TRUE(is_guest_login("guest", "guest"));
    EXPECT_FALSE(is_guest_login("guest", "Guest"));
    EXPECT_FALSE(is_guest_login("Guest", "guest"));
    EXPECT_FALSE(is_guest_login("guest", ""));
    EXPECT_FALSE(is_guest_login("", "guest"));
    EXPECT_FALSE(is_guest_login("10001", "guest"));
    EXPECT_FALSE(is_guest_login(nullptr, "guest"));
    EXPECT_FALSE(is_guest_login("guest", nullptr));
}

TEST_F(LoginFlowTest, AllocGuestAccount_UsesNegativeIds)
{
    int before = g_guest_account;
    EXPECT_EQ(-(before + 1), alloc_guest_account());
    EXPECT_EQ(-(before + 2), alloc_guest_account());
}

TEST_F(LoginFlowTest, SetupGuestUser_FillsMinimalProfileInRk)
{
    CUser user;
    user.set("id", "-77");
    setup_guest_user(&user);

    EXPECT_EQ(1, user.query("guest")) << "游客标记";
    EXPECT_STREQ("rk", user.querystr("start_city")) << "起始地为游戏大厅";
    EXPECT_EQ(10, user.query("str"));
    EXPECT_EQ(10, user.query("int"));
    EXPECT_EQ(10, user.query("con"));
    EXPECT_EQ(10, user.query("dex"));
    EXPECT_EQ(14, user.query("age"));
    EXPECT_EQ(1, user.query("level"));
    EXPECT_STREQ("游客", user.name(1)) << "临时角色名";
    EXPECT_STREQ("-77", user.id(1)) << "临时账号 id 保留";
}

TEST_F(LoginFlowTest, OnLogin_Guest_SkipsDbAndLandsInRkRoom)
{
    // DB 全部失败开关：游客登录全程不应触碰 verify_user/load_from_server
    g_mock_db_succeed = false;

    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    EXPECT_TRUE(on_login(0, 0, frame));

    CUser* me = find_online(CurrentGuestId().c_str());
    ASSERT_NE(nullptr, me) << "游客以负数临时账号在线";
    EXPECT_EQ(1, me->query("guest"));
    EXPECT_STREQ("rk", me->querystr("start_city"));
    ASSERT_NE(nullptr, me->environment());
    EXPECT_STREQ("rk", me->environment()->querystr("base_name"))
        << "游客登录后直接进入游戏大厅 rk";

    CleanupRkRoomAndClerk();
}

TEST_F(LoginFlowTest, OnLogin_Guest_DoesNotTouchAccountDb)
{
    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    EXPECT_TRUE(on_login(0, 0, frame));

    EXPECT_FALSE(db->ExecutedContaining("cc_passwd")) << "游客登录不得查询密码";
    EXPECT_FALSE(db->ExecutedContaining("cc_savedata"))
        << "游客登录不得加载档案";

    CleanupRkRoomAndClerk();
}

TEST_F(LoginFlowTest, OnLogin_GuestTwice_TwoDistinctGuestSessions)
{
    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    ASSERT_TRUE(on_login(0, 0, frame));
    std::string first = CurrentGuestId();

    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    ASSERT_TRUE(on_login(0, 0, frame));
    std::string second = CurrentGuestId();

    EXPECT_NE(first, second) << "每次游客登录分配独立临时账号";
    CUser* me1 = find_online(first.c_str());
    CUser* me2 = find_online(second.c_str());
    ASSERT_NE(nullptr, me1);
    ASSERT_NE(nullptr, me2);
    EXPECT_NE(me1, me2) << "两名游客是独立会话，互不顶替";

    CleanupRkRoomAndClerk();
}

TEST_F(LoginFlowTest, OnLogin_GuestWrongPassword_GoesThroughNormalVerify)
{
    // guest +
    // 错误密码：不得走游客通道，回落到正式账号验证（atol("guest")==0）。
    // verify_user 在测试里走 mock 开关（不产生 SQL），用失败返回值 + 不分配
    // 游客账号两条证据证明走的是正式验证。
    g_mock_db_succeed = false;
    int guest_account = g_guest_account;

    MakeFrame("login 8.8.8.8 1.0 guest hunter22 c1 p1", "9.9.9.9");
    EXPECT_FALSE(on_login(0, 0, frame));
    EXPECT_EQ(guest_account, g_guest_account) << "错误密码不得分配游客临时账号";
    EXPECT_EQ(nullptr, find_online(CurrentGuestId().c_str()))
        << "不得产生游客会话";
}

TEST_F(LoginFlowTest, OnLogin_Guest_ReconnectCreatesNewSession)
{
    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    ASSERT_TRUE(on_login(0, 0, frame));
    std::string first = CurrentGuestId();

    // 游客重连：旧会话仍在（由发呆清理回收），新会话拿新账号
    MakeFrame("login 8.8.8.8 1.0 guest guest c1 p1", "9.9.9.9");
    ASSERT_TRUE(on_login(0, 0, frame));
    std::string second = CurrentGuestId();

    EXPECT_NE(first, second);
    EXPECT_NE(nullptr, find_online(first.c_str())) << "旧游客会话未被顶替";
    EXPECT_NE(nullptr, find_online(second.c_str()));

    CleanupRkRoomAndClerk();
}

// =====================================================================
// on_login 重复登录：顶替旧连接，原地复用同一用户对象
// =====================================================================
TEST_F(LoginFlowTest, OnLogin_Reconnect_ReplacesExistingSession)
{
    StateRecordingUser user;
    user.set("id", "10001");
    user.set("name", "测试者");
    // 旧连接的客户端上下文：has_client 为真才会把顶替通知投递出去，
    // ip 温度决定顶替前后 loginlist 记账使用同一把 key
    user.set_temp("client/serial", 0);
    user.set_temp("client/ip_address", "9.9.9.9");
    user.set_temp("client/ip_local", "8.8.8.8");
    g_user_list[user.object_id()] = &user;
    CRoom* room                   = new CRoom();
    room->set_name("测试房", "login_test_room");
    user.move(room);

    MakeFrame(kLoginCmd, "9.9.9.9");
    EXPECT_TRUE(on_login(0, 0, frame));

    // 顶替的是同一对象：不新建用户，旧连接收到顶替通知
    EXPECT_EQ(static_cast<CUser*>(&user), find_online("10001"));
    EXPECT_TRUE(user.has("有人从别处")) << "旧连接应收到顶替通知";
    EXPECT_TRUE(user.has("上次连线")) << "签到信息应发给本人";
    EXPECT_TRUE(user.has("本次连线"));
    // 重连后仍在房间中（env->change_room 原地刷新）
    EXPECT_EQ(static_cast<CContainer*>(room), user.environment());

    user.set_heart_beat(0);
    user.move(nullptr);
    g_user_list.erase(user.object_id());
    delete room;
}

// =====================================================================
// on_login 的两个辅助出口：公告下载与客户端信息刷送
// =====================================================================
TEST_F(LoginFlowTest, DownBulletInfo_LuaPresent_SendsBulletinFrame)
{
    StateRecordingUser user;
    user.set("id", "10008");
    down_bullet_info(&user);
    // 起始标记必发；lua-default/bullet.lua 提供公告行时逐行下发
    EXPECT_TRUE(user.has("&B=")) << "公告开始标记应下发";
}

TEST_F(LoginFlowTest, SendInfoToClient_FlushesFullState)
{
    StateRecordingUser user;
    user.set("id", "10002");
    user.set("name", "测试者");
    CRoom* room = new CRoom();
    room->set_name("测试房", "login_info_room");
    user.move(room);

    CItem* item = new CItem();
    item->set("base_name", "sword");
    item->set("id", "sword");
    item->move(&user);

    send_info_to_client(&user);
    EXPECT_GT(user.sent, 0) << "角色信息/技能/状态/装备应刷送给客户端";

    item->move(nullptr);
    delete item;
    user.move(nullptr);
    delete room;
}

// =====================================================================
// splite_command_parse / on_offline / on_send_online
// =====================================================================
TEST_F(LoginFlowTest, SpliteCommandParse_UnknownUser_ReturnsFalse)
{
    char cmd[] = "999999 look";
    EXPECT_FALSE(splite_command_parse(999999, cmd, (int)strlen(cmd) + 1));
}

TEST_F(LoginFlowTest, SpliteCommandParse_PushesDecodedCommand)
{
    StateRecordingUser user;
    user.set("id", "10004");
    g_user_list[user.object_id()] = &user;

    // 命令串同样走 XOR 编码："<object_id> look"
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%ld look", user.object_id());
    encode_str(cmd);
    EXPECT_TRUE(
        splite_command_parse(user.object_id(), cmd, (int)strlen(cmd) + 1));
    // 队列收到并消费了这条命令；空队列再取返回 0
    user.handle_command();
    EXPECT_EQ(0, user.handle_command());

    g_user_list.erase(user.object_id());
}

TEST_F(LoginFlowTest, OnOffline_KicksOnlineUser)
{
    StateRecordingUser user;
    user.set("id", "10005");
    user.set("name", "测试者");
    user.set_temp("client/serial", 0);
    g_user_list[user.object_id()] = &user;
    CRoom* room                   = new CRoom();
    room->set_name("测试房", "login_off_room");
    user.move(room);

    // 负对照：未知对象号静默返回，不影响在线用户
    on_offline(999999);
    EXPECT_EQ(0, user.query_temp("net_dead"));

    on_offline(user.object_id());
    EXPECT_NE(0, user.query_temp("net_dead")) << "断线标记已置";
    EXPECT_EQ(0, user.query_temp("client/serial")) << "连接序号已清";

    user.set_heart_beat(0);
    user.move(nullptr);
    g_user_list.erase(user.object_id());
    delete room;
}

TEST_F(LoginFlowTest, OnSendOnline_NotifiesSelf)
{
    StateRecordingUser user;
    user.set("id", "10006");
    on_send_online(&user);
    EXPECT_TRUE(user.has("O=1")) << "上线通知应发给本人";
}

// =====================================================================
// Lua 绑定：在线用户列表导出
// =====================================================================
TEST_F(LoginFlowTest, GetAllOnlineUsers_ReturnsTableOfUsers)
{
    lua_State* L = ensure_lua_env();
    ASSERT_NE(nullptr, L);

    StateRecordingUser user;
    user.set("id", "10007");
    g_user_list[user.object_id()] = &user;

    int top = lua_gettop(L);
    EXPECT_EQ(1, l_get_all_online_users(L));
    EXPECT_TRUE(lua_istable(L, -1));
    lua_rawgeti(L, -1, 1);  // 下标 1：第一个在线用户
    EXPECT_TRUE(lua_isuserdata(L, -1));
    lua_settop(L, top);

    g_user_list.erase(user.object_id());
}

// ---------------- GateClient.cpp: open_client 绑定玩家与登录态（29-33）
// ----------------
TEST_F(LoginFlowTest, OpenClientBindsPlayerAndLoginState)
{
    CGateInfo gate;
    gate.m_player = 0;
    gate.m_login  = 0;

    open_client(reinterpret_cast<uint32_t>(&gate), 4242);

    EXPECT_EQ(4242, gate.m_player) << "serial 反解并绑定玩家(30)";
    EXPECT_EQ(GATE_LOGIN_NORMAL, gate.m_login) << "登录态置正常(31)";
}

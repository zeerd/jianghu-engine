// test_resident.cpp - 居民区 CResident（src/room/Resident.h）的行为契约
//
// CResident 是头文件类（ROOM_BEGIN 宏），测试直接实例化并注册进
// g_room_list；房屋经 load_room("house_<属区>_<户主>") 工厂化为 CHouse，
// 存档/锁/删档走 MockDatabase。菜单与 say 的投递经 MenuRecordingChar 捕获。
// 房屋存档行不带 belong 字段：belong 为空时 CHouse::check_pays 直接返回，
// 避免建房时的扣费逻辑干扰 CResident 用例断言。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "Box.h"
#include "GameInit.h"   // g_current_time / g_start_room
#include "HeartBeat.h"  // g_destruct_list：DESTRUCT_OB 后回收自毁对象
#include "House.h"
#include "Item.h"
#include "Login.h"  // find_online
#include "Mapping.h"
#include "Money.h"
#include "Resident.h"
#include "Room.h"
#include "RoomMgr.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_npc_cleanup.h"
#include "test_online_user.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {

// 固定维护时钟：用例内以 g_current_time 为基准构造"上次付费时间"
constexpr int kNow = 1700000000;

// 物品工厂：box→CBox，coin→CMCoin（已 create），其余 CItem。
// CMoney::player_pay/pay_player 经 g_item_mgr->load_item("coin") 取钱。
CContainer* resident_item_factory(const char* base_name)
{
    CContainer* ob = nullptr;
    if (strcmp(base_name, "box") == 0)
        ob = new CBox();
    else if (strcmp(base_name, "coin") == 0) {
        CMCoin* coin = new CMCoin();
        coin->create();
        ob = coin;
    }
    else {
        ob = new CItem();
    }
    ob->set("base_name", base_name);
    return ob;
}

// 捕获投递内容的角色：菜单/say 均经 send_command，可断言文案
class MenuRecordingChar : public CChar {
public:
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override
    {
        if (msg) msgs.emplace_back(msg);
    }
    bool has(const char* needle) const
    {
        for (const auto& m : msgs) {
            if (m.find(needle) != std::string::npos) return true;
        }
        return false;
    }
    void clear() { msgs.clear(); }

    std::vector<std::string> msgs;
};

// 失败诊断：把已投递文案拼进断言输出（便于判断走了哪个分支）
std::string DumpMsgs(const MenuRecordingChar& c)
{
    std::string s;
    for (auto& m : c.msgs) s += m + " || ";
    return s;
}

}  // namespace

class ResidentTest : public ::testing::Test {
protected:
    MockDatabase* db       = nullptr;
    CMockItemMgr* mgr      = nullptr;
    IItemMgr* saved_mgr    = nullptr;
    CResident* resident    = nullptr;
    int saved_current_time = 0;
    // 用例内造的物件（标记物/现金），测试体结束前销毁
    std::vector<CContainer*> m_items;
    // SetUp 时延迟析构列表的快照：TearDown 只回收本用例新增的自毁对象
    std::vector<CContainer*> m_live_at_setup;
    // 已删房体指针：防止 g_room_list 与 g_destruct_list 双登记导致双删
    std::vector<CContainer*> m_deleted_rooms;
    // AddHouseEntries 的键序号：保证多次追加不覆盖
    int m_house_seq = 0;

    // 预置某房屋的存档行（obj_id 精确匹配）。行内不含 belong：
    // belong 为空时建房触发的 CHouse::check_pays 直接返回，不干扰断言。
    // 必须带 base_name 与 infinity：load() 会整体替换 dbase（copy_mapping
    // 先清空）。生产存档序列化整个 dbase，天然含这两项——缺 base_name 会让
    // destruct 时 remove_room 摘不掉房间；缺 infinity 会让 get_local_house
    // 的二次 load 抹掉建房标记，人物 move 进房被重量检查拒绝。
    void PresetHouseArchive(const char* house_key, const char* owner,
                            int32_t size)
    {
        CMapping archive;
        archive.set("base_name",
                    (std::string("house_测试城_") + house_key).c_str());
        archive.set("name", "雅室");
        archive.set("owner", owner);
        archive.set("size", size);
        archive.set("infinity", 1);
        db->SetQueryResultMatching(
            std::string("obj_id='house_测试城_") + house_key + "'",
            MockDatabase::MakeSingleRow({archive.get_buffer()}));
    }

    // 造一份住户登记（house/<户主> 子映射）。owner 字段语义与生产
    // update_register 一致：写的是第二房主（check_second_owner 据此配菜单）。
    void RegisterHouseEntry(const char* owner, int days_behind,
                            const char* name         = "雅室",
                            const char* short_name   = "房主",
                            const char* second_owner = "")
    {
        CMapping m;
        m.set("time", static_cast<long>(kNow - days_behind * 86400));
        m.set("name", name);
        m.set("short", short_name);
        m.set("owner", second_owner);
        char key[64];
        resident->set(snprintf(key, 64, "house/%s", owner), m);
    }

    // 在属区登记 count 户（can_apply_house/house_price 的户数门槛）。
    // 键带全局序号：多次追加互不覆盖。
    void AddHouseEntries(int count)
    {
        char key[32];
        for (int i = 0; i < count; i++) {
            CMapping m;
            m.set("short", "住户");
            snprintf(key, 32, "house/h%04d", m_house_seq++);
            resident->set(key, m);
        }
    }

    CContainer* MakeItem(const char* base_name)
    {
        CItem* it = new CItem();
        it->set("base_name", base_name);
        it->set("id", base_name);
        m_items.push_back(it);
        return it;
    }

    // 纯 delete、不 move：物件可能挂在已析构的栈角色身上（environment
    // 悬垂），move(nullptr) 会踩 UAF；析构不读 environment，delete 安全。
    void FreeItems()
    {
        for (auto* it : m_items) {
            delete it;
        }
        m_items.clear();
    }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        saved_current_time = g_current_time;
        g_current_time     = kNow;
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        db   = new MockDatabase();
        g_db = db;
        // lock_data(1) 忽略失败直接返 1；lock_data(0) 查到已锁行才立即成功
        // ——不预置 obj_lock 则每次解锁忙等约 4 秒（见 test_house.cpp）
        db->SetQueryResultMatching("select obj_lock",
                                   MockDatabase::MakeSingleRow({"1"}));
        resident = new CResident();
        resident->set("base_name", "resident_test_a");
        resident->set_name("测试住宅区", "resident_test_a");
        resident->set("resident", "测试城");
        g_room_list["resident_test_a"] = resident;
        saved_mgr                      = g_item_mgr;
        mgr                            = new CMockItemMgr();
        mgr->load_factory              = &resident_item_factory;
        g_item_mgr                     = mgr;
    }

    void TearDown() override
    {
        // 1) 先摘仆人：此时房屋仍活着，且房内物件全活（move 遍历列表安全）
        while (true) {
            CNpc* npc = find_npc_by_base_name("monster_yongren");
            if (!npc) break;
            discard_npc(npc);
        }
        // 2) 用例自造的物件（纯 delete，不碰悬垂宿主）
        FreeItems();
        // 3) load_room 造出的房屋：柜子先 destruct 进延迟列表，房体再删。
        // 被 DESTRUCT_OB 的房（如卖房/注销）已由 destruct 摘表并进延迟列表，
        // 不得在此重复删除——记录已删指针供第 4 步跳过。
        std::vector<std::string> house_keys;
        for (auto& kv : g_room_list) {
            if (kv.first.rfind("house_", 0) == 0)
                house_keys.push_back(kv.first);
        }
        for (auto& key : house_keys) {
            CRoom* r = g_room_list[key];
            r->remove_all_box();
            g_room_list.erase(key);
            m_deleted_rooms.push_back(r);
            delete r;
        }
        // 4) 延迟析构列表：只回收本用例新增、且未在上面删过的对象
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            for (auto it = g_destruct_list.begin();
                 it != g_destruct_list.end();) {
                CContainer* ob   = *it;
                bool live_before = false;
                for (auto* p : m_live_at_setup) {
                    if (p == ob) live_before = true;
                }
                if (live_before) {  // 用例前已存在：原样保留
                    ++it;
                    continue;
                }
                it                = g_destruct_list.erase(it);
                bool deleted_here = false;
                for (auto* p : m_deleted_rooms) {
                    if (p == ob) deleted_here = true;
                }
                if (!deleted_here) delete ob;  // 第 3 步已删的只摘列表
            }
        }
        // 5) 摘属区与 mgr/db，恢复时钟
        g_room_list.erase("resident_test_a");
        delete resident;
        resident   = nullptr;
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr  = nullptr;
        g_db = nullptr;
        delete db;
        db             = nullptr;
        g_current_time = saved_current_time;
    }
};

// =====================================================================
// query_attr / count_days / house_price / can_apply_house：属区基础查询
// =====================================================================
TEST_F(ResidentTest, QueryAttr_CanSaveAndIsResident)
{
    EXPECT_EQ(1, resident->query_attr(CMainObject::CAN_SAVE));
    EXPECT_EQ(1, resident->query_attr(CMainObject::IS_RESIDENT));
    EXPECT_EQ(1, resident->query_attr(CMainObject::IS_ROOM));
}

TEST_F(ResidentTest, CountDays_ComputesFromLastPayTime)
{
    // 无登记：0 天
    EXPECT_EQ(0, resident->count_days("owner1"));

    RegisterHouseEntry("owner1", 3);
    EXPECT_EQ(3, resident->count_days("owner1"));
    EXPECT_EQ(0, resident->count_days("ghost"));
}

TEST_F(ResidentTest, HousePrice_ScalesWithHouseCount)
{
    // 无登记：缺省价
    EXPECT_EQ(500000, resident->house_price());

    // 户数/5 定价档：5 户 → 档 1
    AddHouseEntries(5);
    EXPECT_EQ(600000, resident->house_price());

    // 超档封顶：75 户 → 档 15 > 14 → 1300 万
    AddHouseEntries(70);
    EXPECT_EQ(13000000, resident->house_price());
}

TEST_F(ResidentTest, CanApplyHouse_CountLimit)
{
    EXPECT_EQ(1, resident->can_apply_house());

    AddHouseEntries(5);
    EXPECT_EQ(1, resident->can_apply_house()) << "未满 70 户仍可申请";

    AddHouseEntries(65);
    EXPECT_EQ(0, resident->can_apply_house()) << "满 70 户停止申请";
}

// =====================================================================
// set_house：登记写入/注销（注销后属区不再有该户）
// =====================================================================
TEST_F(ResidentTest, SetHouse_RegisterThenDelete)
{
    CMapping m;
    m.set("short", "房主");
    EXPECT_EQ(1, resident->set_house("owner1", &m));
    ASSERT_NE(nullptr, resident->querymap("house/owner1"));
    EXPECT_STREQ("房主", resident->querymap("house/owner1")->querystr("short"));

    EXPECT_EQ(1, resident->set_house("owner1", nullptr));
    EXPECT_EQ(nullptr, resident->querymap("house/owner1"));
}

// =====================================================================
// handle_action：房管命令分发（每条命令落进对应处理函数）
// =====================================================================
TEST_F(ResidentTest, HandleAction_RoutesEveryHouseCommand)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.set("age", 20L);
    me.set("resident", "测试城");
    me.move(resident);

    char comm[32], arg[64], tar[16] = "";

    strcpy(comm, "enter");
    strcpy(arg, "owner1");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("你要进入的房间不在本区！")) << "enter → backhome";

    strcpy(comm, "applyhouse");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("你已经申请成为")) << "applyhouse → applyhouse";

    strcpy(comm, "applyroom");
    strcpy(arg, "2");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("申请房间需要")) << "applyroom → applyroom";

    strcpy(comm, "housename");
    strcpy(arg, "a");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("对不起")) << "housename → housename";

    strcpy(comm, "housedesc");
    strcpy(arg, "");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("简洁漂亮")) << "housedesc → housedesc";

    strcpy(comm, "registerhouse");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("请给您的房间起个名字"))
        << "registerhouse → registerhouse";

    strcpy(comm, "addmaster");
    strcpy(arg, "ghost1");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("请这个人在线时")) << "addmaster → addmaster";

    strcpy(comm, "checkhouse");
    me.clear();
    EXPECT_EQ(0, resident->handle_action(comm, &me, arg, tar))
        << "checkhouse → checkhouse（无住户返回 0）";

    strcpy(comm, "sellhouse");
    me.clear();
    EXPECT_EQ(1, resident->handle_action(comm, &me, arg, tar));
    EXPECT_TRUE(me.has("你要卖掉的房间不在本区！")) << "sellhouse → sellhouse";

    strcpy(comm, "unknown_command");
    me.clear();
    EXPECT_EQ(0, resident->handle_action(comm, &me, arg, tar))
        << "未知命令落回 CRoom::handle_action";

    me.move(nullptr);
}

// =====================================================================
// check_second_owner：他人名下房产才进"进入"菜单
// =====================================================================
TEST_F(ResidentTest, CheckSecondOwner_OffersEnterMenuForOthersHome)
{
    // house/<户主> 映射的 owner 字段 = 第二房主（生产 update_register 语义）：
    // 第二房主获得进入"别人名下房产"的菜单，房主本人不重复出现。
    RegisterHouseEntry("owner1", 0, "雅室", "房主", "wife1");

    MenuRecordingChar wife;
    wife.set("id", "wife1");
    resident->check_second_owner(&wife);
    EXPECT_TRUE(wife.has("进入雅室"));
    EXPECT_TRUE(wife.has("$0enter 0 owner1"));

    MenuRecordingChar stranger;
    stranger.set("id", "stranger1");
    resident->check_second_owner(&stranger);
    EXPECT_FALSE(stranger.has("进入雅室")) << "非第二房主不出现他人房产入口";
}

// =====================================================================
// do_look：住户/非住户/邀请三条视线分支
// =====================================================================
TEST_F(ResidentTest, DoLook_NoHouses_ShowsApplyMenu)
{
    MenuRecordingChar me;
    me.set("id", "stranger1");
    me.move(resident);

    EXPECT_EQ(1, resident->do_look(&me));
    EXPECT_TRUE(me.has("没有任何住户"));
    EXPECT_TRUE(me.has("申请成为测试城居民"));
    EXPECT_FALSE(me.has("回家"));
    EXPECT_FALSE(me.has("查看有哪些住户"));

    me.move(nullptr);
}

TEST_F(ResidentTest, DoLook_ResidentSeesHomeAndSellMenus)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);

    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident);

    EXPECT_EQ(1, resident->do_look(&me));
    EXPECT_TRUE(me.has("住宅区。目前这里有1家住户"));
    EXPECT_TRUE(me.has("回家"));
    EXPECT_TRUE(me.has("查看有哪些住户"));
    EXPECT_TRUE(me.has("卖掉")) << "名下有房时出现卖房入口";
    EXPECT_FALSE(me.has("申请成为")) << "本区住户不再见申请入口";

    me.move(nullptr);
}

TEST_F(ResidentTest, DoLook_InviteTempOffersVisitMenu)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);
    ScopedOnlineUser online("owner1", "房主");

    MenuRecordingChar me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident);

    EXPECT_EQ(1, resident->do_look(&me));
    EXPECT_TRUE(me.has("去房主的家"));
    EXPECT_TRUE(me.has("$0enter 0 owner1"));

    me.move(nullptr);
}

// =====================================================================
// applyhouse / need_money / applyroom / menulist：购房申请流
// =====================================================================
TEST_F(ResidentTest, Applyhouse_GuardsAndMenu)
{
    MenuRecordingChar resident_me;
    resident_me.set("id", "owner1");
    resident_me.set("resident", "测试城");
    EXPECT_EQ(1, resident->applyhouse(&resident_me));
    EXPECT_TRUE(resident_me.has("你已经申请成为"));

    MenuRecordingChar minor;
    minor.set("id", "kid1");
    minor.set("age", 15L);
    EXPECT_EQ(1, resident->applyhouse(&minor));
    EXPECT_TRUE(minor.has("你还没成年"));

    MenuRecordingChar adult;
    adult.set("id", "adult1");
    adult.set("age", 20L);
    EXPECT_EQ(1, resident->applyhouse(&adult));
    EXPECT_TRUE(adult.has("申请单人间"));
    EXPECT_TRUE(adult.has("申请双人套房"));
}

TEST_F(ResidentTest, NeedMoney_HalvesForSingleRoom)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    EXPECT_EQ(500000, resident->need_money(&me));

    me.set_temp("house/size", 1);
    EXPECT_EQ(250000, resident->need_money(&me)) << "单人房半价";
}

TEST_F(ResidentTest, Applyroom_RequiresEnoughCoin)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    // 无现金：按全价提示退回
    EXPECT_EQ(1, resident->applyroom(&me, "2"));
    EXPECT_TRUE(me.has("申请房间需要500000金"));

    // 现金不足：同样退回
    CContainer* coin = MakeItem("coin");
    coin->set("value", 100L);
    coin->move(&me);
    me.clear();
    EXPECT_EQ(1, resident->applyroom(&me, "2"));
    EXPECT_TRUE(me.has("申请房间需要500000金"));

    // 单人房折半后现金足够 → 进入房间名设置菜单
    coin->set("value", 250000L);
    me.clear();
    EXPECT_EQ(1, resident->applyroom(&me, "1"));
    EXPECT_TRUE(me.has("设置房间名称"));
    EXPECT_TRUE(me.has("设置房间描述"));
    EXPECT_FALSE(me.has("设置第二房主")) << "单人房不提供第二房主";

    me.move(nullptr);
}

TEST_F(ResidentTest, Menulist_AdvertsSecondOwnerAndApply)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    resident->menulist(&me);
    EXPECT_TRUE(me.has("设置房间名称"));
    EXPECT_TRUE(me.has("设置房间描述"));
    EXPECT_FALSE(me.has("设置第二房主"));
    EXPECT_FALSE(me.has("申请房间")) << "未起名时不出现登记入口";

    me.clear();
    me.set_temp("house/size", 2);
    me.set_temp("house/name", "雅室");
    resident->menulist(&me);
    EXPECT_TRUE(me.has("设置第二房主")) << "双人房可设第二房主";
    EXPECT_TRUE(me.has("申请房间")) << "起名后出现登记入口";
}

// =====================================================================
// housename / housedesc / addmaster：房间属性设置
// =====================================================================
TEST_F(ResidentTest, Housename_RejectsIllegalAndSetsTemp)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    char bad[] = "a";
    EXPECT_EQ(1, resident->housename(&me, bad));
    EXPECT_TRUE(me.has("对不起"));
    EXPECT_EQ('\0', me.querystr_temp("house/name")[0]);

    char good[] = "雅室";
    me.clear();
    EXPECT_EQ(1, resident->housename(&me, good));
    EXPECT_STREQ("雅室", me.querystr_temp("house/name"));
}

TEST_F(ResidentTest, Housedesc_GuardsLength)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    char empty[] = "";
    EXPECT_EQ(1, resident->housedesc(&me, empty));
    EXPECT_TRUE(me.has("简洁漂亮"));
    EXPECT_EQ('\0', me.querystr_temp("house/desc")[0]);

    std::string long_desc(501, 'x');
    me.clear();
    EXPECT_EQ(1, resident->housedesc(&me, &long_desc[0]));
    EXPECT_TRUE(me.has("简洁漂亮"));
    EXPECT_EQ('\0', me.querystr_temp("house/desc")[0]);

    char good[] = "竹影横窗";
    me.clear();
    EXPECT_EQ(1, resident->housedesc(&me, good));
    EXPECT_STREQ("竹影横窗", me.querystr_temp("house/desc"));
}

TEST_F(ResidentTest, Addmaster_RequiresTargetOnline)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    char ghost[] = "ghost1";
    EXPECT_EQ(1, resident->addmaster(&me, ghost));
    EXPECT_TRUE(me.has("请这个人在线时"));
    EXPECT_EQ('\0', me.querystr_temp("house/owner")[0]);

    ScopedOnlineUser friend_online("friend1", "好友");
    char friend_id[] = "friend1";
    me.clear();
    EXPECT_EQ(1, resident->addmaster(&me, friend_id));
    EXPECT_STREQ("friend1", me.querystr_temp("house/owner"));
}

// =====================================================================
// checkhouse：住户名录
// =====================================================================
TEST_F(ResidentTest, Checkhouse_ListsRegisteredHomes)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    EXPECT_EQ(0, resident->checkhouse(&me)) << "无住户返回 0";

    RegisterHouseEntry("owner1", 0, "雅室", "房主");
    me.clear();
    EXPECT_EQ(1, resident->checkhouse(&me));
    EXPECT_TRUE(me.has("雅室(房主)"));
    EXPECT_TRUE(me.has("$0enter 0 owner1"));
}

// =====================================================================
// registerhouse：登记入住（起名/名额/资金/落库/属区注册/称号）
// =====================================================================
TEST_F(ResidentTest, Registerhouse_RequiresRoomName)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("age", 20L);

    EXPECT_EQ(1, resident->registerhouse(&me));
    EXPECT_TRUE(me.has("请给您的房间起个名字"));
    EXPECT_EQ(nullptr, resident->querymap("house/owner1"));
}

TEST_F(ResidentTest, Registerhouse_RejectsWhenAreaFull)
{
    AddHouseEntries(70);

    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("age", 20L);
    me.set_temp("house/name", "雅室");

    EXPECT_EQ(1, resident->registerhouse(&me));
    EXPECT_TRUE(me.has("本居民区已无空房"));
}

TEST_F(ResidentTest, Registerhouse_RejectsWithoutFunds)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("age", 20L);
    me.set_temp("house/name", "雅室");

    EXPECT_EQ(1, resident->registerhouse(&me));
    EXPECT_TRUE(me.has("现金不足"));
    EXPECT_EQ(nullptr, resident->querymap("house/owner1"));
}

TEST_F(ResidentTest, Registerhouse_SuccessRegistersAndPromotesTitle)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.set("age", 20L);
    me.set("title", "普通百姓");
    me.set_temp("house/name", "雅室");
    me.set_temp("house/desc", "竹影横窗");
    me.set_temp("house/size", 2L);
    me.move(resident);

    CMCoin* coin = new CMCoin();
    coin->create();  // set_name 只写 name/id；生产工厂另写 base_name
    coin->set("base_name", "coin");
    coin->set_amount(500000);
    coin->move(&me);
    m_items.push_back(coin);

    int rc = resident->registerhouse(&me);
    EXPECT_EQ(1, rc) << DumpMsgs(me);

    // 现金被收走：全额付清时 set_amount(0) 自毁现金对象并从身上摘除
    EXPECT_EQ(nullptr, me.present("coin")) << "现金应被收走";
    if (!me.present("coin")) {
        // coin 已自毁进延迟列表（TearDown 统一回收），m_items 不得再删
        m_items.pop_back();
    }
    EXPECT_EQ('\0', me.querystr_temp("house/name")[0]) << "临时申请数据清空";
    EXPECT_STREQ("测试城", me.querystr("resident"));
    EXPECT_STREQ("测试城百姓", me.querystr("title"));

    // 房屋实例已按申请建好并注册进属区
    ASSERT_NE(nullptr, resident->querymap("house/owner1")) << DumpMsgs(me);
    auto it = g_room_list.find("house_测试城_owner1");
    ASSERT_NE(g_room_list.end(), it);
    CRoom* house = it->second;
    EXPECT_STREQ("雅室", house->name());
    EXPECT_STREQ("竹影横窗", house->querystr("desc"));
    EXPECT_EQ(2, house->query("size"));
    EXPECT_STREQ("owner1", house->querystr("owner"));
    EXPECT_EQ('\0', house->querystr("second_owner")[0]);

    me.move(nullptr);
}

// =====================================================================
// sellhouse：估价/成交/注销
// =====================================================================
TEST_F(ResidentTest, Sellhouse_NotRegisteredInArea)
{
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("resident", "测试城");

    EXPECT_EQ(1, resident->sellhouse(&me, "0"));
    EXPECT_TRUE(me.has("你要卖掉的房间不在本区！"));
}

TEST_F(ResidentTest, Sellhouse_HouseGoneRejected)
{
    RegisterHouseEntry("owner1", 0);
    // 不预置房屋存档：get_my_house 拿到 fail_ob 房 → 已被卖掉
    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("resident", "测试城");

    EXPECT_EQ(0, resident->sellhouse(&me, "0"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("已经卖掉"));
}

TEST_F(ResidentTest, Sellhouse_NotOwnerRejected)
{
    RegisterHouseEntry("intruder", 0);
    PresetHouseArchive("intruder", "owner1", 2);  // 存档户主是别人

    MenuRecordingChar me;
    me.set("id", "intruder");
    me.set("resident", "测试城");

    EXPECT_EQ(0, resident->sellhouse(&me, "0"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这不是你的房子"));
}

TEST_F(ResidentTest, Sellhouse_QuotesHalfPriceValue)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);

    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident);

    // 房价 50 万，双人房不折半，卖出只值一半 = 25 万；当天卖无税
    EXPECT_EQ(1, resident->sellhouse(&me, "0")) << DumpMsgs(me);
    EXPECT_TRUE(me.has("目前价值250000"));
    EXPECT_TRUE(me.has("决定卖了"));
    EXPECT_EQ(nullptr, me.present("coin")) << "确认阶段不得付款";
    EXPECT_NE(nullptr, resident->querymap("house/owner1")) << "未成交不注销";

    me.move(nullptr);
}

TEST_F(ResidentTest, Sellhouse_CompletePaysAndRemovesHouse)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);

    MenuRecordingChar me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident);

    EXPECT_EQ(1, resident->sellhouse(&me, "1")) << DumpMsgs(me);
    EXPECT_TRUE(me.has("你卖掉了"));

    // 款项到账、注册注销、房屋实例销毁、离线房主临时档回收
    CContainer* coin = me.present("coin");
    ASSERT_NE(nullptr, coin);
    EXPECT_EQ(250000, coin->query_amount());
    m_items.push_back(coin);  // 收款进 m_items，TearDown 统一回收
    EXPECT_EQ(nullptr, resident->querymap("house/owner1"));
    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "destruct 应按 base_name 摘掉房间";
    EXPECT_EQ(nullptr, find_online("owner1"));

    me.move(nullptr);
}

// =====================================================================
// backhome：进入房屋的权限与在场校验
// =====================================================================
TEST_F(ResidentTest, Backhome_RejectsUnknownHouse)
{
    MenuRecordingChar me;
    me.set("id", "owner1");

    char arg[] = "owner1";
    EXPECT_EQ(1, resident->backhome(&me, arg));
    EXPECT_TRUE(me.has("你要进入的房间不在本区！"));
}

TEST_F(ResidentTest, Backhome_RejectsIntruder)
{
    RegisterHouseEntry("owner1", 0);

    MenuRecordingChar me;
    me.set("id", "stranger1");

    char arg[] = "owner1";
    EXPECT_EQ(1, resident->backhome(&me, arg));
    EXPECT_TRUE(me.has("你试图闯入"));
}

TEST_F(ResidentTest, Backhome_SelfButHouseMissingRejected)
{
    RegisterHouseEntry("owner1", 0);
    // 不预置存档：房屋工厂置 fail_ob，load 失败 → 此房间不存在

    MenuRecordingChar me;
    me.set("id", "owner1");

    char arg[] = "owner1";
    EXPECT_EQ(1, resident->backhome(&me, arg));
    EXPECT_TRUE(me.has("此房间不存在"));
}

TEST_F(ResidentTest, Backhome_InvitedGuestBlockedWhenMasterAway)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);

    MenuRecordingChar me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident);

    char arg[] = "owner1";
    EXPECT_EQ(1, resident->backhome(&me, arg));
    EXPECT_TRUE(me.has("主人不在家")) << "受邀但主人不在场：不得闯入";

    me.move(nullptr);
}

TEST_F(ResidentTest, Backhome_InvitedGuestEntersWhenMasterHome)
{
    RegisterHouseEntry("owner1", 0);
    PresetHouseArchive("owner1", "owner1", 2);
    // 房间登记进 g_room_list 由夹具统一回收（先摘仆人再删房），
    // 不用局部 ScopedRoom——房先于 NPC 析构会让仆人 environment 悬垂
    CRoom* house_room = load_room("house_测试城_owner1");

    // 房内放置"id=owner1"的物件代表主人在家
    CContainer* master_marker = MakeItem("master_marker");
    master_marker->set("id", "owner1");
    master_marker->move(house_room);

    MenuRecordingChar me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident);

    char arg[] = "owner1";
    EXPECT_EQ(1, resident->backhome(&me, arg)) << DumpMsgs(me);
    ASSERT_NE(nullptr, me.environment()) << "受邀客人应已入房 " << DumpMsgs(me);
    EXPECT_EQ(static_cast<CContainer*>(house_room), me.environment())
        << "受邀客人移入房屋 " << DumpMsgs(me);

    me.move(nullptr);
    master_marker->move(nullptr);
}

// =====================================================================
// clear_house：欠费收回（不足 30 天不动、扣款、余额不足、离线装载）
// =====================================================================
TEST_F(ResidentTest, ClearHouse_NoRegisteredHousesDoesNothing)
{
    resident->clear_house();
    EXPECT_TRUE(db->GetExecutedSqls().empty()) << "无登记时不产生任何库操作";
}

TEST_F(ResidentTest, ClearHouse_RecentDebtIsNotTouched)
{
    RegisterHouseEntry("owner1", 5);
    resident->clear_house();

    EXPECT_NE(nullptr, resident->querymap("house/owner1"));
    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "未到期不触碰房屋实例";
}

TEST_F(ResidentTest, ClearHouse_DeductsRentFromOnlineBalance)
{
    PresetHouseArchive("owner1", "owner1", 2);
    RegisterHouseEntry("owner1", 31);
    ScopedOnlineUser owner("owner1", "房主");
    owner.get()->set("balance", 10000000L);

    resident->clear_house();

    // 欠 31 天 × 50 万 / 250 = 6.2 万，银行余额扣清后房主保有房产
    CUser* u = find_online("owner1");
    ASSERT_NE(nullptr, u) << "在线房主不被销毁";
    EXPECT_EQ(10000000 - 62000, u->query("balance"));

    auto it = g_room_list.find("house_测试城_owner1");
    ASSERT_NE(g_room_list.end(), it);
    EXPECT_EQ(g_current_time, it->second->query("pay_time")) << "付费时间刷新";
    EXPECT_NE('\0', it->second->querystr("long")[0])
        << "注册回写更新了房间描述";
    EXPECT_NE(nullptr, resident->querymap("house/owner1"))
        << "扣款成功：注册保留";
}

TEST_F(ResidentTest, ClearHouse_SmallHouseHalvesRent)
{
    PresetHouseArchive("owner1", "owner1", 1);  // 单人房租金减半
    RegisterHouseEntry("owner1", 31);
    ScopedOnlineUser owner("owner1", "房主");
    owner.get()->set("balance", 100L);

    resident->clear_house();

    // 半租 3.1 万：余额 100 扣成负数且不足付清 → 只扣款不收房
    CUser* u = find_online("owner1");
    ASSERT_NE(nullptr, u);
    EXPECT_EQ(100 - 31000, u->query("balance"));
    EXPECT_NE(nullptr, resident->querymap("house/owner1"));
}

TEST_F(ResidentTest, ClearHouse_OfflineOwnerReclaimedAfterRentPass)
{
    PresetHouseArchive("owner1", "owner1", 2);
    RegisterHouseEntry("owner1", 31);

    resident->clear_house();

    // 离线房主被临时装载、扣款后销毁回收
    EXPECT_EQ(nullptr, find_online("owner1"));
    EXPECT_NE(nullptr, resident->querymap("house/owner1"));
}

// =====================================================================
// remove_house：注销并销毁房屋（离线房主档案回收）
// =====================================================================
TEST_F(ResidentTest, RemoveHouse_ClearsOwnerAndDestroysRoom)
{
    RegisterHouseEntry("owner1", 0);
    CHouse* house = new CHouse();
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    g_room_list["house_测试城_owner1"] = house;

    resident->remove_house(house);

    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "房屋从房间表摘除";
    EXPECT_EQ(nullptr, resident->querymap("house/owner1")) << "注册注销";
    EXPECT_EQ(nullptr, find_online("owner1")) << "离线房主装载后销毁";
    EXPECT_TRUE(db->ExecutedContaining("delete from t_room"))
        << "房屋档案从库中删除";
}

// =====================================================================
// update_register：离线房主装载 + 第二房主分支
// =====================================================================
TEST_F(ResidentTest, UpdateRegister_OfflineOwnersLoadAndDestroy)
{
    CHouse* house = new CHouse();
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    house->set("second_owner", "wife1");
    house->set("pay_time", static_cast<int32_t>(kNow));

    resident->update_register(house);

    ASSERT_NE(nullptr, resident->querymap("house/owner1"));
    EXPECT_STREQ("wife1",
                 resident->querymap("house/owner1")->querystr("owner"));
    EXPECT_EQ(nullptr, find_online("owner1")) << "离线第一房主装载后销毁";
    EXPECT_EQ(nullptr, find_online("wife1")) << "离线第二房主装载后销毁";
    EXPECT_NE(std::string::npos,
              std::string(house->querystr("long")).find("共享房间"));

    delete house;
}

TEST_F(ResidentTest, UpdateRegister_OfflineSingleOwner)
{
    CHouse* house = new CHouse();
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    house->set("pay_time", static_cast<int32_t>(kNow));

    resident->update_register(house);

    ASSERT_NE(nullptr, resident->querymap("house/owner1"));
    EXPECT_EQ('\0', resident->querymap("house/owner1")->querystr("owner")[0])
        << "无第二房主时注册 owner 为空";
    EXPECT_EQ(nullptr, find_online("owner1"));
    EXPECT_NE(std::string::npos,
              std::string(house->querystr("long")).find("私人房间"));

    delete house;
}

// =====================================================================
// setup / reset：维护窗口倒计时到期后执行载入与清欠
// =====================================================================
TEST_F(ResidentTest, Setup_TriggersArchiveLoad)
{
    resident->setup();
    EXPECT_TRUE(db->ExecutedContaining("resident_test_a"))
        << "setup 应触发一次属区档案装载";
}

TEST_F(ResidentTest, Reset_CountsDownThenRunsMaintenance)
{
    // 欠 31 天且无房屋存档：维护窗口到期后 clear_house 注销该户
    RegisterHouseEntry("owner1", 31);
    resident->setup();

    // wait_clear ∈ [40,79]：80 次 reset 必然走完倒计时
    for (int i = 0; i < 80; i++) {
        resident->reset();
    }
    EXPECT_EQ(nullptr, resident->querymap("house/owner1"))
        << "到期维护执行了 clear_house（欠费户被注销）";
}

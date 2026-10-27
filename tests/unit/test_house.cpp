// test_house.cpp - 玩家私人房间（lua-default/room/house_base.lua）的行为
// 契约
//
// 房屋以 house_ 前缀动态实例房承载：测试用 CLuaRoom("house_base") 驱动
// 脚本钩子，断言与 C++ 时代逐条对齐。
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <initializer_list>
#include <string>
#include <vector>

#include "Box.h"
#include "Item.h"
#include "LuaRoom.h"  // 房屋与属区都是 Lua 房间
#include "Mapping.h"
#include "NpcMgr.h"
#include "Room.h"
#include "RoomMgr.h"
#include "User.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_npc_cleanup.h"
#include "HeartBeat.h"  // g_destruct_list 快照回收
#include "Money.h"      // CMCoin/CMoney（房租现金支）
#include "test_online_user.h"
#include "test_sample_script.h"
#include "test_state_recorder.h"  // 帐单/门牌菜单观察面

extern IDatabase* g_db;
extern char g_notify_fail_msg[512];
extern MAP2USER g_user_list;
extern int g_current_time;  // 房租天数/缴费时刻的确定性锚点

namespace {

// call_house_script 的实参：对象 / 字符串（原样字节，编码由调用方负责）
struct lua_arg {
    enum class kind { object, text, number };

    kind k          = kind::number;
    CMainObject* ob = nullptr;
    const char* s   = nullptr;
    long n          = 0;

    static lua_arg of(CMainObject* o)
    {
        lua_arg a;
        a.k  = kind::object;
        a.ob = o;
        return a;
    }

    static lua_arg text(const char* t)
    {
        lua_arg a;
        a.k = kind::text;
        a.s = t;
        return a;
    }
};

// 调用某房屋脚本表的方法（self 即该房屋）。返回其整数结果；若结果是对象
// 则同时写进 *out_obj（可传 nullptr 表示不关心）。
long call_room_script(CLuaRoom* room, const char* name, CMainObject** out_obj,
                      std::initializer_list<lua_arg> args)
{
    if (out_obj) *out_obj = nullptr;

    lua_State* L = g_LuaEngine->get_state();
    int ref      = room->get_script_ref();
    if (!L || ref == LUA_NOREF) return 0;

    CLuaActiveRefGuard guard(ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    lua_bind_push_object(L, room);
    int nargs = 1;
    for (const auto& a : args) {
        switch (a.k) {
            case lua_arg::kind::object:
                lua_bind_push_object(L, a.ob);
                break;
            case lua_arg::kind::text:
                lua_pushstring(L, a.s ? a.s : "");
                break;
            case lua_arg::kind::number:
                lua_pushinteger(L, a.n);
                break;
        }
        ++nargs;
    }

    long result = 0;
    if (lua_pcall(L, nargs, 1, 0) == LUA_OK) {
        if (out_obj && lua_type(L, -1) == LUA_TUSERDATA)
            *out_obj = lua_bind_check_object(L, -1);
        result = (long)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    else {
        fprintf(stderr, "[lua] %s raised: %s\n", name,
                lua_tostring(L, -1) ? lua_tostring(L, -1) : "(no message)");
        lua_pop(L, 1);
    }
    lua_pop(L, 1);  // 弹出脚本表
    return result;
}

// add_box 期望 g_item_mgr 交出真 CBox（生产路径 ItemMgr 造 CBox）；
// 工厂不记账所有权——箱子 move 进房后由房间析构回收。
CContainer* box_factory(const char* base_name)
{
    CContainer* ob = nullptr;
    if (strcmp(base_name, "box") == 0)
        ob = new CBox();
    else
        ob = new CItem();
    ob->set("base_name", base_name);
    return ob;
}

// 现款工厂：房租支付经 g_item_mgr->load_item("coin") 供货
CContainer* coin_factory(const char* base_name)
{
    if (strcmp(base_name, "coin") != 0) return box_factory(base_name);
    auto* c = new CMCoin();
    c->create();  // 出厂值 base_value=1
    c->set("base_name", base_name);
    return c;
}

}  // namespace

// 每用例独占 MockDatabase；挂/摘干净，避免悬垂 g_db。
// 房子必须堆分配：房间先于 NPC 析构会让仆人的 environment 悬垂，
// 之后对仆人 move(nullptr) 即 UAF——因此 TearDown 严格按
// 「先摘 NPC（脱离房间）→ 再删房 → 最后摘 mgr/db」的顺序回收。
class HouseTest : public ::testing::Test {
protected:
    MockDatabase* db  = nullptr;
    CMockItemMgr* mgr = nullptr;
    // 用例内造的物件（租品/目标物），测试体结束前脱房销毁
    std::vector<CContainer*> m_items;

    CContainer* MakeItem(const char* base_name, bool rented)
    {
        CItem* it = new CItem();
        it->set("base_name", base_name);
        it->set("id", base_name);
        if (rented) it->set_temp("yongren/rent", 1);
        m_items.push_back(it);
        return it;
    }

    // 纯 delete、不 move：物品可能挂在已析构的栈角色身上（environment
    // 悬垂），move(nullptr) 会踩 UAF；而析构不读 environment，delete 安全。
    // 前提：调用时房内 NPC 已先摘（NPC 的 move 会遍历房内列表，要求物品全活）。
    void FreeItems()
    {
        for (auto* it : m_items) {
            delete it;
        }
        m_items.clear();
    }

    IItemMgr* saved_mgr = nullptr;
    CLuaRoom* h         = nullptr;
    CLuaRoom* resident  = nullptr;
    std::vector<CContainer*> m_live_at_setup;  // 延迟析构快照

    // 摘掉房内仆役：create 补的仆役若留着，会让依赖「无仆役」的用例失真；
    // 房体 delete 后仆役的 environment 悬垂，摘除必须先于删房。
    void discard_all_servants()
    {
        while (true) {
            CNpc* npc = find_npc_by_base_name("monster_yongren");
            if (!npc) break;
            discard_npc(npc);
        }
    }

    // 直接调用房屋脚本函数（self 为 h）。返回其整数结果；若结果是对象则
    // 同时写进 *out_obj（可传 nullptr 表示不关心）。
    long call_house_script(const char* name, CMainObject** out_obj,
                           std::initializer_list<lua_arg> args)
    {
        return call_room_script(h, name, out_obj, args);
    }

    // check_rent 的返回值语义：nil=无租品，userdata=租品本身
    CContainer* check_rent_of(CChar* who)
    {
        CMainObject* obj = nullptr;
        call_house_script("check_rent", &obj, {lua_arg::of(who)});
        return dynamic_cast<CContainer*>(obj);
    }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        db   = new MockDatabase();
        g_db = db;
        // lock_data(1) 实现忽略失败直接返 1；lock_data(0) 查到已锁行才立即
        // 成功——不预置 obj_lock 则每次解锁忙等约 4 秒
        db->SetQueryResultMatching("select obj_lock",
                                   MockDatabase::MakeSingleRow({"1"}));
        // housename/housedesc 走 load()：预置存档行（与 h 内存态字段一致）
        CMapping archive;
        archive.set("owner", "owner1");
        archive.set("belong", "resident_test_a");
        archive.set("size", 2);
        db->SetQueryResultMatching(
            "select obj_data from t_room",
            MockDatabase::MakeSingleRow({archive.get_buffer()}));
        // belong 属区：update_register/check_pays 要求 belong 是活的
        // IS_RESIDENT 房间——测试住宅区脚本安装通用居民区逻辑
        resident = new CLuaRoom("fixture_resident_room");
        resident->set("base_name", "resident_test_a");
        resident->create();  // 脚本 create 写入区名/城名/场景描述
        g_room_list["resident_test_a"] = resident;
        saved_mgr                      = g_item_mgr;
        mgr                            = new CMockItemMgr();
        mgr->load_factory              = &box_factory;
        g_item_mgr                     = mgr;
        h                              = new CLuaRoom("house_base");
        h->set("base_name", "house_test_room");
        h->set("belong", "resident_test_a");
        // 直接调脚本表方法需脚本引用已生效；CLuaRoomT 仅在 create/reset 内
        // 加载引用。这里用生产同款 create，再把可观察状态复位成 C++ 时代的
        // 基线（标准房间名、无柜子、无仆役），让各用例按原基线断言。
        h->create();
        h->set_name("标准房间");
        h->remove_all_box();
        discard_all_servants();
        h->set_temp("yongren", 0);
    }

    void TearDown() override
    {
        // 延迟析构：remove_me 推入的房体等本用例新对象统一回收
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
        // 1) 先摘仆人：此时 h 仍活着，且房内物品全活（move 遍历列表安全）
        while (true) {
            CNpc* npc = find_npc_by_base_name("monster_yongren");
            if (!npc) break;
            discard_npc(npc);
        }
        // 2) 再删用例物件（纯 delete，不碰悬垂宿主），最后删房（断开列表）
        FreeItems();
        delete h;
        h = nullptr;
        // 3) 摘属区与 mgr/db
        g_room_list.erase("resident_test_a");
        delete resident;
        resident   = nullptr;
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr  = nullptr;
        g_db = nullptr;
        delete db;
        db = nullptr;
    }
};

// ---------------------------------------------------------------------
// 属性：可存盘（CAN_SAVE），其余委托 CRoom
// ---------------------------------------------------------------------
TEST_F(HouseTest, QueryAttr_CanSaveAndDelegatesToRoom)
{
    EXPECT_EQ(1, h->query_attr(CMainObject::CAN_SAVE));
    EXPECT_EQ(1, h->query_attr(CMainObject::IS_ROOM));
}

// ---------------------------------------------------------------------
// create()：默认值 + 单柜 + 缺省仆人（combat_exp=20000，记入 temp）
// ---------------------------------------------------------------------
TEST_F(HouseTest, Create_SetsDefaultsAndSpawnsServant)
{
    h->create();

    EXPECT_EQ(1, h->query("sleep_room"));
    EXPECT_STREQ("house", h->querystr("area"));
    EXPECT_EQ(1, h->query("saveobjects"));
    EXPECT_EQ(20, h->query("max_items"));

    // 仆人：load_npc("monster_yongren") 入房，temp 记 id，缺省 20000 经验
    // （TearDown 统一摘除：先于房间删除，避免 environment 悬垂）
    const int32_t servant_id = h->query_temp("yongren");
    ASSERT_NE(0, servant_id);
    CNpc* npc = find_npc_by_base_name("monster_yongren");
    ASSERT_NE(nullptr, npc);
    EXPECT_EQ(20000, npc->query("combat_exp"));
    EXPECT_EQ(static_cast<CContainer*>(h), npc->environment());
}

// ---------------------------------------------------------------------
// create()：box_count=N -> N 个柜子，柜内 max_items=50 且禁装人
// ---------------------------------------------------------------------
TEST_F(HouseTest, Create_BoxCountSpawnsExtraBoxes)
{
    h->set("box_count", 3);
    h->create();

    int box_count = 0;
    for (auto* obj : *h->get_item_list()) {
        if (!EQUALSTR(obj->querystr("base_name"), "box")) continue;
        box_count++;
        EXPECT_EQ(50, obj->query("max_items"));
        EXPECT_EQ(1, obj->query("no_drop_man"));
    }
    EXPECT_EQ(3, box_count);
}

// ---------------------------------------------------------------------
// create()：yongren 映射字段落到仆人身上（名字/欢迎词）
// ---------------------------------------------------------------------
TEST_F(HouseTest, Create_AppliesYongrenMapping)
{
    h->set("yongren/name", "仆人");
    h->set("yongren/welcome", "主人回来了。");
    h->create();

    CNpc* npc = find_npc_by_base_name("monster_yongren");
    ASSERT_NE(nullptr, npc);
    EXPECT_STREQ("仆人", npc->name());
    EXPECT_STREQ("主人回来了。", npc->querystr("welcome"));
}

// ---------------------------------------------------------------------
// setup()：按 size 出对应地图 + 出口门指向 belong
// ---------------------------------------------------------------------
TEST_F(HouseTest, Setup_SetsMapBySizeAndExitDoor)
{
    CLuaRoom* small = new CLuaRoom("house_base");
    small->set("base_name", "house_test_setup1");
    small->set("belong", "resident_test_a");
    small->set("size", 1);
    small->create();  // 加载脚本引用
    small->setup();
    EXPECT_STREQ("玩家民居1", small->querystr("map"));
    CDoor* door = small->present_door("出口");
    ASSERT_NE(nullptr, door);
    EXPECT_STREQ("resident_test_a", door->querystr("next_room"));
    EXPECT_STREQ("out", door->querystr("dir"));

    CLuaRoom* big = new CLuaRoom("house_base");
    big->set("base_name", "house_test_setup2");
    big->set("belong", "resident_test_a");
    big->set("size", 2);
    big->create();
    big->setup();
    EXPECT_STREQ("玩家民居2", big->querystr("map"));

    // 仆役先于房体摘除：房体 delete 后 environment 悬垂
    discard_all_servants();
    delete big;
    delete small;
}

// =====================================================================
// check_rent：主人/第二主人豁免；外人持租品才被查住
// =====================================================================
TEST_F(HouseTest, CheckRent_OwnerAndSecondOwnerExempt)
{
    h->set("owner", "owner1");
    h->set("second_owner", "wife1");

    CChar stranger;
    stranger.set("id", "stranger1");
    CContainer* rent = MakeItem("rent_sword", true);
    rent->move(&stranger);

    // 主人/第二主人：即使拿着租品也放行
    CChar owner;
    owner.set("id", "owner1");
    rent->move(&owner);
    EXPECT_EQ(nullptr, check_rent_of(&owner));

    CChar second;
    second.set("id", "wife1");
    rent->move(&second);
    EXPECT_EQ(nullptr, check_rent_of(&second));

    // 外人：持租品 -> 返回该物件；空手 -> nullptr
    rent->move(&stranger);
    EXPECT_EQ(rent, check_rent_of(&stranger));
    rent->move(nullptr);
    EXPECT_EQ(nullptr, check_rent_of(&stranger));
}

// =====================================================================
// handle_action：cast force roar 只在仆人在场时被截获（任何身份）
// =====================================================================
TEST_F(HouseTest, HandleAction_RoarInterceptedOnlyWithServant)
{
    h->set("owner", "owner1");
    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    char comm[] = "cast";
    char arg[]  = "force roar";
    char tar[]  = "";

    // 无仆人：放行（落回 CRoom::handle_action -> 0）
    EXPECT_EQ(0, h->handle_action(comm, &stranger, arg, tar));

    // 仆人在场：任何身份都截获
    h->create();
    EXPECT_EQ(1, h->handle_action(comm, &stranger, arg, tar));

    CChar owner;
    owner.set("id", "owner1");
    owner.set("name", "房主");
    owner.move(h);
    EXPECT_EQ(1, h->handle_action(comm, &owner, arg, tar));

    owner.move(nullptr);
    stranger.move(nullptr);
}

// =====================================================================
// handle_action：外人 + 仆人在场 -> kill/fight/cast/join/guard 全截获；
// 同命令主人或无租品外人放行（落回基类 0）
// =====================================================================
TEST_F(HouseTest, HandleAction_CombatCommandsBlockedForRentingStranger)
{
    h->set("owner", "owner1");
    h->create();  // 仆人在场

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    CContainer* rent = MakeItem("rent_sword", true);
    rent->move(&stranger);  // 外人持有租品

    CContainer* target = MakeItem("dummy_target", false);
    target->move(h);  // 房内可 present 的目标
    char tar[16];
    snprintf(tar, 16, "%d", target->object_id());

    for (const char* c : {"kill", "fight", "cast", "join", "guard"}) {
        char comm[16];
        strncpy(comm, c, sizeof(comm));
        char arg[] = "";
        EXPECT_EQ(1, h->handle_action(comm, &stranger, arg, tar))
            << "外人持租品应被拦截: " << c;
    }

    // 主人同命令：跳过外人截获块，落回基类 -> 0
    CChar owner;
    owner.set("id", "owner1");
    owner.set("name", "房主");
    owner.move(h);
    char comm2[] = "kill";
    char arg2[]  = "";
    EXPECT_EQ(0, h->handle_action(comm2, &owner, arg2, tar));

    // 外人空手（无租品）：不拦 -> 0
    rent->move(nullptr);
    EXPECT_EQ(0, h->handle_action(comm2, &stranger, arg2, tar));

    owner.move(nullptr);
    stranger.move(nullptr);
}

// =====================================================================
// handle_action：外人对租品的 drop/get/sell 截获；give 给仆人放行
// =====================================================================
TEST_F(HouseTest, HandleAction_RentItemActionsBlocked)
{
    h->set("owner", "owner1");
    h->create();

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    // 身上租品：drop / sell 截获
    CContainer* worn = MakeItem("rent_sword", true);
    worn->move(&stranger);

    char comm_d[] = "drop";
    char arg_d[]  = "";
    char tar_w[16];
    snprintf(tar_w, 16, "%d", worn->object_id());
    EXPECT_EQ(1, h->handle_action(comm_d, &stranger, arg_d, tar_w));

    char comm_s[] = "sell";
    EXPECT_EQ(1, h->handle_action(comm_s, &stranger, tar_w, tar_w));

    // 房内租品：get 截获；普通物件 get 放行
    CContainer* floor_rent = MakeItem("rent_dagger", true);
    floor_rent->move(h);
    char comm_g[] = "get";
    char arg_g[]  = "";
    char tar_f[16];
    snprintf(tar_f, 16, "%d", floor_rent->object_id());
    EXPECT_EQ(1, h->handle_action(comm_g, &stranger, arg_g, tar_f));

    CContainer* floor_plain = MakeItem("plain_rock", false);
    floor_plain->move(h);
    char tar_p[16];
    snprintf(tar_p, 16, "%d", floor_plain->object_id());
    EXPECT_EQ(0, h->handle_action(comm_g, &stranger, arg_g, tar_p));

    stranger.move(nullptr);
}

// =====================================================================
// handle_action：外人携 transfer_tie 用 trans/use 被截获（需持租品）
// =====================================================================
TEST_F(HouseTest, HandleAction_TransferItemIntercepted)
{
    h->set("owner", "owner1");
    h->create();

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    CContainer* tie = MakeItem("transfer_tie", false);
    tie->move(&stranger);
    char tar_t[16];
    snprintf(tar_t, 16, "%d", tie->object_id());

    char comm_u[] = "use";
    char arg_u[]  = "";
    char comm_t[] = "trans";

    // 无租品：放行
    EXPECT_EQ(0, h->handle_action(comm_u, &stranger, arg_u, tar_t));

    // 持租品：use/trans 都截获
    CContainer* rent = MakeItem("rent_sword", true);
    rent->move(&stranger);
    EXPECT_EQ(1, h->handle_action(comm_u, &stranger, arg_u, tar_t));
    EXPECT_EQ(1, h->handle_action(comm_t, &stranger, arg_u, tar_t));

    stranger.move(nullptr);
}

// =====================================================================
// valid_leave：外人带租品从「出口」离开被拦；主人/空手/非出口放行
// =====================================================================
TEST_F(HouseTest, ValidLeave_RentItemBlockedAtExitOnly)
{
    h->set("owner", "owner1");
    h->create();

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    char exit_dir[]  = "出口";
    char other_dir[] = "北";

    // 非出口方向：不查租品
    EXPECT_EQ(1, h->valid_leave(&stranger, other_dir));

    // 出口 + 无租品：放行
    EXPECT_EQ(1, h->valid_leave(&stranger, exit_dir));

    // 出口 + 持租品：拦截（notify_fail 返回 0，提示留下）
    CContainer* rent = MakeItem("rent_sword", true);
    rent->move(&stranger);
    EXPECT_EQ(0, h->valid_leave(&stranger, exit_dir));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("留下"));

    // 主人带租品：豁免
    CChar owner;
    owner.set("id", "owner1");
    owner.set("name", "房主");
    rent->move(&owner);
    EXPECT_EQ(1, h->valid_leave(&owner, exit_dir));

    owner.move(nullptr);
    stranger.move(nullptr);
}

// =====================================================================
// do_kick：踢出在线访客——租用品掉落房内、人移回 belong、清邀请标记
// （在线用户登记用共享的 ScopedOnlineUser，见 test_online_user.h）
// =====================================================================

TEST_F(HouseTest, DoKick_EjectsVisitorAndDropsRentedItems)
{
    h->set("owner", "owner1");
    h->create();

    CUser* guest = nullptr;
    {
        ScopedOnlineUser online("guest1", "访客");
        guest = online.get();
        guest->set_temp("invite", "owner1");
        guest->move(h);

        // 访客身上带着租用品
        CContainer* rent = MakeItem("rent_sword", true);
        rent->move(guest);

        CChar me;
        me.set("id", "owner1");
        me.set("name", "房主");
        me.move(h);

        char arg[] = "guest1";
        EXPECT_EQ(1, h->handle_action((char*)"kickout", &me, arg, (char*)""));

        // 租用品留在房内，访客回 belong 属区，邀请标记被清
        EXPECT_EQ(static_cast<CContainer*>(h), rent->environment());
        EXPECT_EQ(static_cast<CContainer*>(resident), guest->environment());
        EXPECT_EQ('\0', guest->querystr_temp("invite")[0]);

        me.move(nullptr);
        guest->move(nullptr);
    }
}

TEST_F(HouseTest, DoKick_Rejections)
{
    h->set("owner", "owner1");

    CChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.move(h);

    // 不在线：notify_fail -> 0
    char nobody[] = "ghost1";
    EXPECT_EQ(0, h->handle_action((char*)"kickout", &me, nobody, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你想踢谁"));

    // 踢房主：拒绝 -> 0
    ScopedOnlineUser owner_online("owner1", "房主");
    char self_id[] = "owner1";
    EXPECT_EQ(0, h->handle_action((char*)"kickout", &me, self_id, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("人家可是房主"));

    // 在线但不在房内：拒绝登门 -> 1
    ScopedOnlineUser away("guest2", "路人");
    char away_id[] = "guest2";
    EXPECT_EQ(1, h->handle_action((char*)"kickout", &me, away_id, (char*)""));

    me.move(nullptr);
}

// =====================================================================
// addmaster：非户主/单人房/离线目标全部拒绝；在线成功设第二房主并回写注册
// =====================================================================
TEST_F(HouseTest, Addmaster_GuardsAndSuccess)
{
    h->set("owner", "owner1");
    h->set("size", 2);

    // 完全外人：房管命令不进分发，落基类 -> 0（addmaster 内部的
    // “只有户主”防线实际只拦第二主人——他们在 handle_action 的 else 分支里）
    CChar outsider;
    outsider.set("id", "stranger1");
    char arg1[] = "friend1";
    EXPECT_EQ(0,
              h->handle_action((char*)"addmaster", &outsider, arg1, (char*)""));
    EXPECT_EQ('\0', h->querystr("second_owner")[0]);

    // 第二主人越权发起：addmaster 内部拒绝 -> 1，second_owner 不变
    h->set("second_owner", "wife1");
    CChar second;
    second.set("id", "wife1");
    EXPECT_EQ(1,
              h->handle_action((char*)"addmaster", &second, arg1, (char*)""));
    EXPECT_STREQ("wife1", h->querystr("second_owner"));
    h->del("second_owner");

    // 单人房：拒绝
    h->set("size", 1);
    CChar owner;
    owner.set("id", "owner1");
    EXPECT_EQ(1, h->handle_action((char*)"addmaster", &owner, arg1, (char*)""));
    EXPECT_EQ('\0', h->querystr("second_owner")[0]);
    h->set("size", 2);

    // 目标不在线：拒绝
    EXPECT_EQ(1, h->handle_action((char*)"addmaster", &owner, arg1, (char*)""));
    EXPECT_EQ('\0', h->querystr("second_owner")[0]);

    // 目标在线 + 房主发起：设第二房主并 update_register 回写属区
    ScopedOnlineUser owner_online("owner1", "房主");
    ScopedOnlineUser friend_online("friend1", "好友");
    EXPECT_EQ(1, h->handle_action((char*)"addmaster", &owner, arg1, (char*)""));
    EXPECT_STREQ("friend1", h->querystr("second_owner"));
    // 住宅区脚本 update_register 把注册项写进属区 house 表
    EXPECT_NE(nullptr, resident->querymap("house/owner1"));
}

// =====================================================================
// housename：非法名拒绝；合法名改名并回写注册（load 预置行保障）
// =====================================================================
TEST_F(HouseTest, Housename_RejectsIllegalAndRenames)
{
    h->set("owner", "owner1");

    CChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.move(h);

    // 非法名（长度不足）：拒绝且名字保持 CRoom 默认（"标准房间"）
    char bad[] = "a";
    EXPECT_EQ(1, h->handle_action((char*)"housename", &me, bad, (char*)""));
    EXPECT_STREQ("标准房间", h->name()) << "拒绝后不得改名";

    // 合法名：load 成功 -> set_name -> update_register
    ScopedOnlineUser owner_online("owner1", "房主");
    char good[] = "雅室";
    EXPECT_EQ(1, h->handle_action((char*)"housename", &me, good, (char*)""));
    EXPECT_STREQ("雅室", h->name());

    me.move(nullptr);
}

// =====================================================================
// 临时：name 污染追踪（定位后删除）
// =====================================================================
// （已定位：CRoom 构造默认 set_name("标准房间")，h 出生自带默认名）

// =====================================================================
// housedesc：长度越界/引号拒绝；合法写入 desc 并 save
// =====================================================================
TEST_F(HouseTest, Housedesc_GuardsAndPersist)
{
    h->set("owner", "owner1");

    CChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.move(h);

    // 空描述：拒绝
    char empty[] = "";
    EXPECT_EQ(1, h->handle_action((char*)"housedesc", &me, empty, (char*)""));
    EXPECT_EQ('\0', h->querystr("desc")[0]);

    // 超长（>500）：拒绝
    std::string long_desc(501, 'x');
    EXPECT_EQ(
        1, h->handle_action((char*)"housedesc", &me, &long_desc[0], (char*)""));
    EXPECT_EQ('\0', h->querystr("desc")[0]);

    // 含引号：valid_string 拦 -> 0
    char quoted[] = "有\"引号\"";
    EXPECT_EQ(0, h->handle_action((char*)"housedesc", &me, quoted, (char*)""));

    // 合法：load -> set desc -> save
    ScopedOnlineUser owner_online("owner1", "房主");
    char good[] = "竹影横窗";
    EXPECT_EQ(1, h->handle_action((char*)"housedesc", &me, good, (char*)""));
    EXPECT_STREQ("竹影横窗", h->querystr("desc"));

    me.move(nullptr);
}

// ====================================================================
// 户主门牌 / 帐单播报 / 交租链（do_look、do_paymoney 成功支）
// ====================================================================
TEST_F(HouseTest, DoLook_OwnerSeesRoomMenu)
{
    h->set("owner", "owner1");
    h->set("size", 2);
    auto* owner = new StateRecordingUser();
    owner->set_name("房主", "owner1");
    owner->set("id", "owner1");
    owner->set("hp", (long)100);
    owner->move(h);
    owner->delete_temp("client/loading");

    EXPECT_EQ(1, h->do_look(owner)) << "门牌：长desc+短desc播报+菜单(427-443)";
    EXPECT_TRUE(owner->has("拒绝客人来访")) << "户主菜单项(433)";
    EXPECT_TRUE(owner->has("邀请其他人同住")) << "大房主才见同住邀请(437-438)";

    owner->move(nullptr);
    delete owner;
}

TEST_F(HouseTest, Paymoney_BillView_ShowsSettlement)
{
    h->set("owner", "owner1");
    h->set("need_pays", (long)100);
    auto* owner = new StateRecordingUser();
    owner->set_name("房主", "owner1");
    owner->set("id", "owner1");
    owner->set("hp", (long)100);
    owner->move(h);
    owner->delete_temp("client/loading");

    EXPECT_EQ(1,
              h->handle_action((char*)"paymoney", owner, (char*)"", (char*)""))
        << "paymoney 派发(242-243)";
    EXPECT_TRUE(owner->has("管理费用结算单")) << "帐单播报(507-513)";
    EXPECT_TRUE(owner->has("结算")) << "结算菜单项(514-515)";

    owner->move(nullptr);
    delete owner;
}

TEST_F(HouseTest, Paymoney_PayPath_DeductsCashAndUpdates)
{
    const int saved_time = g_current_time;
    g_current_time       = 1000000;
    // 户主 10 天前登记 → 房租价>0；在线但银行存款不足 → check_pays 先挂帐
    CMapping pays;
    pays.set("time", (long)(1000000 - 10 * 86400));
    resident->set("house/owner1", pays);

    // load() 会整体替换 dbase → 重排 db 模式：obj_lock + 与夹具一致的存档
    CMapping archive;
    archive.set("owner", "owner1");
    archive.set("belong", "resident_test_a");
    archive.set("size", 2);
    db->Clear();
    db->SetQueryResultMatching("select obj_lock",
                               MockDatabase::MakeSingleRow({"1"}));
    db->SetQueryResultMatching(
        "select obj_data from t_room",
        MockDatabase::MakeSingleRow({archive.get_buffer()}));

    h->set("owner", "owner1");
    h->set("need_pays", (long)100);  // 505 前置；load 后由 check_pays 重算挂帐
    ScopedOnlineUser owner("owner1", "房主");

    mgr->load_factory = &coin_factory;
    ASSERT_EQ(1, CMoney::pay_player(owner.get(), 100000))
        << "先备现金（房租 20000 量级）";

    EXPECT_EQ(1, h->handle_action((char*)"paymoney", owner.get(), (char*)"1",
                                  (char*)""))
        << "交租全链(518-543)";
    EXPECT_EQ(0, h->query("need_pays")) << "结清(533)";
    EXPECT_EQ(1000000, h->query("pay_time")) << "记缴费时刻(534)";

    CContainer* coin = owner.get()->present_name("coin", CChar::IS_MONEY);
    if (coin) {
        coin->set_heart_beat(0);
        coin->move(nullptr);
        delete coin;
    }
    mgr->load_factory = &box_factory;
    g_current_time    = saved_time;
}

// ====================================================================
// check_pays：在线户主自动扣租 / 非居住区早退
// ====================================================================
TEST_F(HouseTest, CheckPays_OnlineOwnerAutoDeducts)
{
    const int saved_time = g_current_time;
    g_current_time       = 1000000;
    CMapping pays;
    pays.set("time", (long)(1000000 - 10 * 86400));
    resident->set("house/owner1", pays);
    h->set("owner", "owner1");

    ScopedOnlineUser owner("owner1", "房主");
    owner.get()->set("balance", (long)1000000);

    call_house_script("check_pays", nullptr, {});

    const long bal = owner.get()->query("balance");
    EXPECT_LT(bal, 1000000) << "银行余额自动扣租";
    EXPECT_GT(bal, 900000) << "扣额=房租（size<2 减半，万金量级）";
    EXPECT_EQ(0, h->query("need_pays")) << "扣成不挂帐";
    EXPECT_EQ(1000000, h->query("pay_time")) << "记缴费时刻";

    g_current_time = saved_time;
}

TEST_F(HouseTest, CheckPays_NonResidentZone_KeepsBill)
{
    auto* zone = new CRoom();
    zone->set_name("非居住区", "nr_zone");
    g_room_list["nr_zone"] = zone;

    h->set("belong", "nr_zone");
    h->set("need_pays", (long)77);
    call_house_script("check_pays", nullptr, {});
    EXPECT_EQ(77, h->query("need_pays")) << "非居住区 sys 播报后早退，帐单原封";

    g_room_list.erase("nr_zone");
    delete zone;
}

// 离线户主：check_pays 临时载入用户；存档查不到 → 余额不足 → 挂帐
TEST_F(HouseTest, CheckPays_OfflineOwnerKeepsBillWhenLoadFails)
{
    const int saved_time = g_current_time;
    g_current_time       = 1000000;
    CMapping pays;
    pays.set("time", (long)(1000000 - 10 * 86400));
    resident->set("house/owner1", pays);
    h->set("owner", "owner1");

    call_house_script("check_pays", nullptr, {});  // 无在线用户 → 临时载档

    EXPECT_GT((long)h->query("need_pays"), 0) << "未扣成挂帐";

    // 临时用户由 check_pays 延迟析构，先从用户表摘除，避免悬垂键
    for (auto it = g_user_list.begin(); it != g_user_list.end();) {
        if (it->second && strcmp(it->second->querystr("id"), "owner1") == 0)
            it = g_user_list.erase(it);
        else
            ++it;
    }
    g_current_time = saved_time;
}

// 属区不是居住区：update_register 只播报，不写住户登记
TEST_F(HouseTest, UpdateRegister_NonResidentZoneSkipsRegistry)
{
    auto* zone = new CRoom();
    zone->set_name("非居住区", "nr_zone");
    g_room_list["nr_zone"] = zone;

    h->set("belong", "nr_zone");
    h->set("owner", "owner1");
    call_house_script("update_register", nullptr, {});

    EXPECT_EQ(nullptr, resident->querymap("house/owner1"))
        << "非居住区不写登记";

    g_room_list.erase("nr_zone");
    delete zone;
}

// 交租时房屋存档已不存在：安排延迟析构并返回 0
TEST_F(HouseTest, Paymoney_MissingArchiveDropsHouse)
{
    db->Clear();
    db->SetQueryResultMatching("select obj_lock",
                               MockDatabase::MakeSingleRow({"1"}));
    h->set("need_pays", (long)100);

    CChar me;
    me.set("id", "owner1");
    EXPECT_EQ(0,
              h->handle_action((char*)"paymoney", &me, (char*)"1", (char*)""))
        << "load 失败 → remove_me(523-526)";
    EXPECT_EQ(100, h->query("need_pays")) << "帐单未动";
}

// ====================================================================
// reset 补仆 / remove_me 延迟析构
// ====================================================================
TEST_F(HouseTest, Reset_SpawnsServantAndChecksPays)
{
    h->reset();
    EXPECT_NE(0, h->query_temp("yongren"))
        << "缺仆即补(73-74)；顺带 check_pays(77-78)";
}

TEST_F(HouseTest, RemoveMe_PushesHouseToDelayedDestruct)
{
    auto* h2 = new CLuaRoom("house_base");
    h2->set_name("待拆房", "house_pending");
    h2->create();  // 加载脚本引用；补的仆役随即摘除，避免析构后悬垂
    discard_all_servants();
    call_room_script(h2, "remove_me", nullptr, {});

    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
        for (auto* ob : g_destruct_list) {
            if (ob == h2) found = true;
        }
    }
    EXPECT_TRUE(found) << "remove_me 推入延迟析构，TearDown 统一回收";
}

// ====================================================================
// 佣人在场的 give/get 截获（转手防丢、捡人防拿）
// ====================================================================
TEST_F(HouseTest, Give_ToYongrenOk_RentItemGuarded)
{
    h->set("owner", "owner1");
    h->create();  // 佣人在场是截获前提(123)

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    CNpc* yn = find_npc_by_base_name("monster_yongren");
    ASSERT_NE(yn, nullptr);

    CContainer* gift = MakeItem("gift_sword", false);
    gift->move(&stranger);
    char tar_g[16];
    snprintf(tar_g, 16, "%d", gift->object_id());
    char arg_y[32];
    snprintf(arg_y, 32, "to %d", yn->object_id());
    EXPECT_EQ(0, h->handle_action((char*)"give", &stranger, arg_y, tar_g))
        << "给用人放行(171-180)";

    CContainer* rent = MakeItem("rent_glaive", true);
    rent->move(&stranger);
    char tar_r[16];
    snprintf(tar_r, 16, "%d", rent->object_id());
    CChar other;
    other.set("id", "other1");
    other.set("name", "同伴");
    other.move(h);
    char arg_o[32];
    snprintf(arg_o, 32, "to %d", other.object_id());
    EXPECT_EQ(1, h->handle_action((char*)"give", &stranger, arg_o, tar_r))
        << "租借品转手被喝止(182-186)";

    other.move(nullptr);
    stranger.move(nullptr);
}

// =====================================================================
// accept_object（monster_yongren.lua）：租借品收还、非租借品拒收。
// E2E 走不到收还分支——do_price 把租品交到仆役手上，玩家背包里按名
// 定位到的只可能是非租品（house/servant.sce 只能断言拒收侧），
// 正向契约在此守护：脚本检查丢失 yongren/rent 时此处必须变红。
// 单测脚本根不含 lua-sample，故用共享夹具把样本脚本原文暂存进
// 脚本根（引擎按内容 hash 自动重载，负缓存见
// LuaConditionTest.LoadFailure_RecoversWhenScriptFileAppears）。
// =====================================================================
TEST_F(HouseTest, ServantAcceptObject_AcceptsRentItemOnly)
{
    ScopedSampleScript script("npc/monster_yongren.lua",
                              "lua-sample/npc/monster_yongren.lua");
    ASSERT_TRUE(script.ok()) << "样本仆役脚本未就位: " << script.file();

    h->set("owner", "owner1");
    h->create();  // 仆役入房：本次 spawn 时样本脚本已就位，脚本引用生效

    CNpc* yn = find_npc_by_base_name("monster_yongren");
    ASSERT_NE(yn, nullptr);

    CChar me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.move(h);

    CContainer* plain = MakeItem("gift_sword", false);
    EXPECT_EQ(0, yn->accept_object(&me, plain))
        << "非租借品必须拒收（与基类缺省一致，防脚本误收）";

    CContainer* rent = MakeItem("rent_saber", true);
    EXPECT_EQ(1, yn->accept_object(&me, rent))
        << "带 yongren/rent 标记的租借品必须收还（脚本分支，基类缺省是 0）";

    rent->move(nullptr);
    plain->move(nullptr);
    me.move(nullptr);
}

TEST_F(HouseTest, Get_CarrierOfRentAndUser_Guarded)
{
    h->set("owner", "owner1");
    h->create();

    CChar stranger;
    stranger.set("id", "stranger1");
    stranger.set("name", "路人");
    stranger.move(h);

    CChar carrier;
    carrier.set("id", "carrier1");
    carrier.set("name", "房客");
    carrier.move(h);
    CContainer* rent = MakeItem("rent_saber", true);
    rent->move(&carrier);
    char tar_c[16];
    snprintf(tar_c, 16, "%d", carrier.object_id());
    EXPECT_EQ(1, h->handle_action((char*)"get", &stranger, (char*)"", tar_c))
        << "带租借品的人不可捡(205-208)";

    auto* guest = new StateRecordingUser();
    guest->set_name("访客", "guest1");
    guest->set("id", "guest1");
    guest->set("hp", (long)100);
    guest->move(h);
    guest->delete_temp("client/loading");
    char tar_u[16];
    snprintf(tar_u, 16, "%d", guest->object_id());
    EXPECT_EQ(1, h->handle_action((char*)"get", &stranger, (char*)"", tar_u))
        << "人不是物品(210-212)";

    guest->move(nullptr);
    delete guest;
    carrier.move(nullptr);
    stranger.move(nullptr);
}

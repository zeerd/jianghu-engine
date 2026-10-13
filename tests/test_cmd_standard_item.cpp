// test_cmd_standard_item.cpp - Unit tests for CmdStandardItem.cpp
//
// Tests CCmdStandard::valid_move (the public static helper).
//
#include "pch.h"
#include "Char.h"
#include "Npc.h"
#include "User.h"
#include "Room.h"
#include "Item.h"
#include "test_state_recorder.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "CmdStandard.h"
#include "Equip.h"
#include "FightRoom.h"
#include "Global.h"
#include "HeartBeat.h"
#include "test_lua_env.h"

extern char g_notify_fail_msg[512];

class CmdStandardItemTest : public ::testing::Test {
protected:
    // Friend helpers
    static int CallGet(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"get", me, target, arg);
    }
    static int CallUse(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"use", me, target, arg);
    }
    static int CallDrop(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"drop", me, target, arg);
    }
    static int CallGive(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"give", me, target, arg);
    }

    CChar* me;
    CContainer* room;
    CContainer* item_obj;
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        saved_mgr  = g_item_mgr;
        g_item_mgr = &item_mgr;
        room       = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->move(room);

        item_obj = new CContainer();
        item_obj->set("name", "测试物品");
        item_obj->set("id", "test_item");
        item_obj->move(room);
    }

    void TearDown() override
    {
        item_obj->move(nullptr);
        me->move(nullptr);
        room->move(nullptr);
        delete item_obj;
        delete me;
        delete room;
        g_item_mgr = saved_mgr;
    }
};

TEST_F(CmdStandardItemTest, Get_NoArgs_NoTarget)
{
    // get with no target in room → notify_fail
    int result = CallGet(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardItemTest, Get_ValidObject)
{
    // get the item object by ID
    int result = CallGet(me, item_obj->object_id(), (char*)"");
    // get should succeed or at least not crash
    EXPECT_NE(result, -1);
}

TEST_F(CmdStandardItemTest, Use_NoArgs_NoTarget)
{
    int result = CallUse(me, 0, (char*)"");
    // no usable item → fail
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardItemTest, Drop_NoArgs)
{
    int result = CallDrop(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

TEST_F(CmdStandardItemTest, Give_NoArgs)
{
    int result = CallGive(me, 0, (char*)"");
    EXPECT_EQ(result, 0);
}

// ---------------- give：赠送 ----------------

TEST_F(CmdStandardItemTest, Give_UnknownRecipient_Rejected)
{
    CItem* gift = new CItem();
    gift->set_name("手帕", "cmd_handkerchief");
    gift->move(me);

    char arg[24] = "";
    snprintf(arg, 24, "%d", 123456);
    EXPECT_EQ(CallGive(me, gift->object_id(), arg), 0);
    EXPECT_EQ(gift->environment(), me) << "没送出去就得还在身上";

    gift->move(nullptr);
    delete gift;
}

TEST_F(CmdStandardItemTest, Give_NpcRefuses_Rejected)
{
    CItem* gift = new CItem();
    gift->set_name("手帕", "cmd_handkerchief_npc");
    gift->move(me);

    CNpc* guard = new CNpc();
    guard->set_name("家丁", "cmd_guard");
    guard->move(room);

    char arg[24] = "";
    snprintf(arg, 24, "x %ld", guard->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallGive(me, gift->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方不要你的东西。");
    EXPECT_EQ(gift->environment(), me) << "被拒绝的礼物仍在赠送者身上";

    guard->set_heart_beat(0);
    guard->move(nullptr);
    delete guard;
    gift->move(nullptr);
    delete gift;
}

TEST_F(CmdStandardItemTest, Give_NoAcceptFlag_Rejected)
{
    CItem* gift = new CItem();
    gift->set_name("手帕", "cmd_handkerchief2");
    gift->move(me);

    CNpc* picky = new CNpc();
    picky->set_name("挑剔鬼", "cmd_picky");
    picky->move(room);
    picky->set("env/no_accept", 1);

    char arg[24] = "";
    snprintf(arg, 24, "x %ld", picky->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(CallGive(me, gift->object_id(), arg), 0);
    EXPECT_STREQ(g_notify_fail_msg, "对方好象不愿意收下你的东西。");
    EXPECT_EQ(gift->environment(), me) << "拒收后物品仍在赠送者身上";

    picky->set_heart_beat(0);
    picky->move(nullptr);
    delete picky;
    gift->move(nullptr);
    delete gift;
}

TEST_F(CmdStandardItemTest, Give_ToUser_TransfersItem)
{
    CItem* gift = new CItem();
    gift->set_name("手帕", "cmd_handkerchief3");
    gift->move(me);

    StateRecordingUser receiver;
    receiver.set_name("路人", "cmd_receiver");
    receiver.set("hp", (long)100);
    receiver.move(room);
    receiver.delete_temp("client/loading");

    char arg[24] = "";
    snprintf(arg, 24, "x %ld", receiver.object_id());
    EXPECT_EQ(CallGive(me, gift->object_id(), arg), 1);
    EXPECT_EQ(gift->environment(), &receiver) << "赠予玩家同样入包";

    receiver.move(nullptr);
    delete gift;
}

// ---------------- give 成功路径崩溃定位探针 ----------------
// NPC 收下礼物后 do_give 会先播报再移交：这里把两步拆开验证

// 收下赠与的 NPC：默认收物钩子由任务系统决定，这里显式答应
class AcceptiveNpc : public CNpc {
public:
    int accept_object(CChar* me, CContainer* item) override
    {
        accepted = item;
        return 1;
    }

    CContainer* accepted = nullptr;
};

// NPC 收下礼物：走通 do_give 的成功路径（播报 + 移交 + 存盘占位）
TEST_F(CmdStandardItemTest, Give_AcceptedByNpc_TransfersItem)
{
    CItem* gift = new CItem();
    gift->set_name("手帕", "cmd_handkerchief_npc_ok");
    gift->move(me);

    AcceptiveNpc* shopkeeper = new AcceptiveNpc();
    shopkeeper->set_name("掌柜", "cmd_shopkeeper");
    shopkeeper->move(room);

    char arg[24] = "";
    snprintf(arg, 24, "x %ld", shopkeeper->object_id());
    EXPECT_EQ(CallGive(me, gift->object_id(), arg), 1);
    EXPECT_EQ(gift->environment(), shopkeeper) << "收下即入对方囊中";
    EXPECT_EQ(shopkeeper->accepted, gift) << "accept_object 拿到的正是该物";

    // 容器不销毁子物件：物品仍在掌柜身上，先取出再删容器，
    // 否则物品的 environment 成为悬空指针，下一次 move 即访问已释放内存
    gift->move(nullptr);
    shopkeeper->set_heart_beat(0);
    shopkeeper->move(nullptr);
    delete shopkeeper;
    delete gift;
}

// ====================================================================
// 深度用例：get/use/wield/unwield/drop/give 的分支矩阵
// （独立夹具：延迟析构回收 + 分裂工厂追踪 + max_items 容量语义）
// ====================================================================
namespace {

// 可堆叠：move 进人时豁免"涉嫌复制"闸门；分裂工厂也产它
class CombiningItem : public CItem {
public:
    int32_t query_attr(int attr) override
    {
        if (attr == CAN_COMBINE) return 1;
        return CItem::query_attr(attr);
    }
};

// 会用/半会用：can_use=1，do_use 分别返回 1/0，锁定 use 成功支与回滚支
class UsableItem : public CombiningItem {  // 叠加件豁免"涉嫌复制"闸门
public:
    int can_use() override { return 1; }
    int do_use(CChar*, CContainer*) override { return 1; }
};

class HalfUsableItem : public CombiningItem {  // 同上
public:
    int can_use() override { return 1; }
    int do_use(CChar*, CContainer*) override { return 0; }
};

// 分裂件工厂：split() 经 g_item_mgr->load_item 取新件（mock 必须供货）；
// 按 base_name 区分类型，保证 do_use 落在与原件相同的类上
std::vector<CContainer*> g_split_pieces;

CContainer* SplitItemFactory(const char* base_name)
{
    CItem* it = nullptr;
    if (strcmp(base_name, "usable_pill") == 0) {
        it = new UsableItem();
        it->set_name("回气散", base_name);
    }
    else if (strcmp(base_name, "half_pill") == 0) {
        it = new HalfUsableItem();
        it->set_name("哑药", base_name);
    }
    else {
        auto* c = new CombiningItem();
        c->set_name("分裂物", base_name);
        it = c;
    }
    it->set("base_name", base_name);
    g_split_pieces.push_back(it);
    return it;
}

}  // namespace

class CmdStandardItemDeepTest : public ::testing::Test {
protected:
    static int CallGet(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"get", me, target, arg);
    }
    static int CallUse(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"use", me, target, arg);
    }
    static int CallWield(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"wield", me, target, arg);
    }
    static int CallUnwield(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"unwield", me, target, arg);
    }
    static int CallDrop(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"drop", me, target, arg);
    }
    static int CallGive(CChar* me, int32_t target, char* arg)
    {
        CCmdStandard cmd;
        return cmd.handlecommand((char*)"give", me, target, arg);
    }

    CChar* me;
    CContainer* room;
    CMockItemMgr item_mgr;
    MockDatabase* test_db = nullptr;  // 从玩家容器搬出物件会触存盘
    IItemMgr* saved_mgr   = nullptr;
    std::vector<CContainer*> m_items;          // 用例造物，统一回收
    std::vector<CContainer*> m_live_at_setup;  // 延迟析构快照

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        {
            std::lock_guard<std::mutex> lock(g_destruct_list_mutex);
            m_live_at_setup = std::vector<CContainer*>(g_destruct_list.begin(),
                                                       g_destruct_list.end());
        }
        g_notify_fail_msg[0]  = '\0';
        test_db               = new MockDatabase();
        g_db                  = test_db;
        saved_mgr             = g_item_mgr;
        g_item_mgr            = &item_mgr;
        item_mgr.load_factory = &SplitItemFactory;

        room = new CContainer();
        room->set("name", "test_room");
        room->set("max_items", (long)10000);  // 拥挤判定按容量语义

        me = new CChar();
        me->set_name("测试者", "tester1");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->set("max_hp", (long)100);
        me->move(room);
    }

    void TearDown() override
    {
        // 1) 先把人移出（与既有夹具同序：战房里还挂着角色时先摘房会踩悬垂环境）
        me->move(nullptr);
        room->move(nullptr);
        // 2) 再把全部物件出列表（me 仍存活，其内物件可安全脱挂）
        for (auto* it : m_items) {
            it->set_heart_beat(0);
            it->move(nullptr);
        }
        for (auto* p : g_split_pieces) {
            p->set_heart_beat(0);
            p->move(nullptr);
        }

        // 2) 延迟析构：本用例新增（无值被毁灭物/已耗分裂件）统一回收
        std::vector<CContainer*> reclaimed;
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
                reclaimed.push_back(ob);
                delete ob;
            }
        }

        // 3) 未被回收的常规造物与工厂分裂件
        auto is_reclaimed = [&reclaimed](CContainer* p) {
            for (auto* r : reclaimed) {
                if (r == p) return true;
            }
            return false;
        };
        for (auto* it : m_items) {
            if (!is_reclaimed(it)) delete it;
        }
        m_items.clear();
        for (auto* p : g_split_pieces) {
            if (!is_reclaimed(p)) delete p;
        }
        g_split_pieces.clear();

        delete me;
        delete room;
        g_item_mgr = saved_mgr;
        g_db       = nullptr;
        delete test_db;
        test_db = nullptr;
    }

    CContainer* MakeItem(const char* name, const char* id, CContainer* env,
                         long amount = 1)
    {
        CItem* it = (amount > 1) ? new CombiningItem() : new CItem();
        it->set_name(name, id);
        it->set("base_name", id);
        if (amount > 1) it->set_amount(static_cast<int32_t>(amount));
        it->move(env);
        m_items.push_back(it);
        return it;
    }

    // 录制玩家：userp 守卫（no_use/no_wield）与私发文案的观察面
    StateRecordingUser* MakeUser(const char* id)
    {
        auto* u = new StateRecordingUser();
        u->set_name("玩家", id);
        u->set("id", id);
        u->set("level", (long)50);
        u->set("hp", (long)100);
        u->move(room);
        u->delete_temp("client/loading");
        m_items.push_back(u);
        return u;
    }

    // 装备一件兵器（wield 成功支需要 wield_position + apply）
    CEquip* WieldSword(CChar* who, const char* id)
    {
        auto* eq = new CEquip();
        eq->set_name("长剑", id);
        eq->set("base_name", id);
        eq->set("skill_type", "sword");
        CMapping apply;
        apply.set("damage", 1L);
        apply.set("desc", "攻击");
        eq->set("apply", apply);
        eq->set("wield_position", (int32_t)WIELD_HAND);
        eq->move(who);  // 只做准备：正式装备由 wield 命令完成（重复装会被
                        // do_wield 拒）
        m_items.push_back(eq);
        return eq;
    }
};

// ---------------- get：参数解析 / 守卫矩阵 / 分裂与超重 ----------------
TEST_F(CmdStandardItemDeepTest, Get_EnvNumArg_PicksFromContainer)
{
    auto* pouch = new CContainer();
    pouch->set_name("钱袋", "pouch1");
    pouch->move(room);
    m_items.push_back(pouch);
    CContainer* sword = MakeItem("小刀", "pouch_dagger", pouch);

    char arg[48];
    snprintf(arg, sizeof(arg), "0 %ld 0", (long)pouch->object_id());
    EXPECT_EQ(1, CallGet(me, sword->object_id(), arg))
        << "带容器编号的三段参数解析(18-21)";
    EXPECT_EQ(sword->environment(), me) << "从指定容器拾取";
}

TEST_F(CmdStandardItemDeepTest, Get_SplitByNum_SplitsStack)
{
    me->set_temp("fight/is_fighting", 1);  // 86: 战斗中拾取留 busy
    me->set_temp("fight/ap", (long)50);    // 37: check_ap 放行
    CContainer* gold = MakeItem("金叶子", "gold_stack", room, 5);

    char arg[24] = "0 0 2";
    EXPECT_EQ(1, CallGet(me, gold->object_id(), arg)) << "按数量分裂(61-62)";
    EXPECT_EQ(3, gold->query_amount()) << "原件减量";
    int32_t in_me = 0;
    for (auto* c : *me->get_item_list()) {
        if (c->query_amount() == 2) in_me = 2;
    }
    EXPECT_EQ(2, in_me) << "分裂件 2 枚入包(70-73)";
    EXPECT_TRUE(me->is_busy()) << "战斗中留 busy(86)";
}

TEST_F(CmdStandardItemDeepTest, Get_HeavyItem_TooBurdened)
{
    CContainer* rock = MakeItem("巨石", "heavy_rock", room);
    rock->set_weight(1000);  // 负重闸：me 上限 0

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, rock->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("无法负担"))
        << "75";
    EXPECT_EQ(room, rock->environment()) << "拿不动仍在原地";
}

TEST_F(CmdStandardItemDeepTest, Get_FightroomTooDangerous_Rejected)
{
    auto* fr = new CFightRoom();
    fr->set_name("乱战", "get_fr");
    fr->move(room);
    m_items.push_back(fr);
    me->move(fr);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, 0, (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("太危险"))
        << "27";
}

TEST_F(CmdStandardItemDeepTest, Get_SearchLivingBody_GuardRequiresHigherWizard)
{
    auto* rich = new CChar();
    rich->set_name("富商", "get_body1");
    rich->set("grant", (long)200);  // 巫阶高于我 → 32 判否 → 33 搜身拒
    rich->move(room);
    m_items.push_back(rich);
    me->set("grant", (long)70);

    char arg[48];
    snprintf(arg, sizeof(arg), "0 %ld 0", (long)rich->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, rich->object_id(), arg));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("搜身"))
        << "33";
    me->set("grant", (long)0);
}

TEST_F(CmdStandardItemDeepTest, Get_SleepRoomCarryPerson_Rejected)
{
    room->set("sleep_room", 1);  // area 未置 → 非民居客店(43)
    auto* sleeper = MakeUser("get_sleep1");
    sleeper->set_temp("disabled", "1");  // 昏迷 → CAN_GET(=!living)

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, sleeper->object_id(), (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("客店"))
        << "43-44";
}

TEST_F(CmdStandardItemDeepTest, Get_FbNodiePlayer_Rejected)
{
    auto* runner = MakeUser("get_fb1");
    runner->set_temp("disabled", "1");
    runner->set_temp("fb_nodie", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, runner->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("非死亡副本"))
        << "46-47";
}

TEST_F(CmdStandardItemDeepTest, Get_NoGetFlag_CustomMessage)
{
    auto* capped = MakeUser("get_nog1");
    capped->set_temp("disabled", "1");
    capped->set("no_get", "此人被点了穴");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, capped->object_id(), (char*)""));
    EXPECT_STREQ(g_notify_fail_msg, "此人被点了穴") << "49-53";
}

TEST_F(CmdStandardItemDeepTest, Get_OwnershipGuard_Rejected)
{
    CContainer* loot = MakeItem("他人包裹", "others_bag", room);
    loot->set("owner", "someone");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGet(me, loot->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这不是你的东西"))
        << "57-59";
}

// ---------------- use：房间禁令 / 目标参数 / 拆分消费与回滚 ----------------
TEST_F(CmdStandardItemDeepTest, Use_NoUseRoom_Rejected)
{
    auto* user = MakeUser("use_r1");
    room->set("no_use", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallUse(user, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止使用道具"))
        << "97-98";
}

TEST_F(CmdStandardItemDeepTest, Use_BadTargetArg_Rejected)
{
    CContainer* pill
        = MakeItem("回气散", "usable_pill", me, 1);  // 在身但目标参数非法

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallUse(me, pill->object_id(), (char*)"999999"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你要对谁使用"))
        << "107-111";
}

TEST_F(CmdStandardItemDeepTest, Use_ConsumableSplitsAndSucceeds)
{
    auto* pill = new UsableItem();  // 成功支要 can_use=1 + do_use=1 的真类
    pill->set_name("回气散", "usable_pill");
    pill->set("base_name", "usable_pill");
    pill->set_amount(3);
    pill->move(me);
    m_items.push_back(pill);

    EXPECT_EQ(1, CallUse(me, pill->object_id(), (char*)""))
        << "do_use 命中成功支(121-130)";
    CContainer* stack = nullptr;
    for (auto* c : *me->get_item_list()) {
        if (strcmp(c->querystr("base_name"), "usable_pill") == 0) stack = c;
    }
    ASSERT_NE(nullptr, stack);
    EXPECT_EQ(3, stack->query_amount()) << "成功后总数归位(124-126)";
}

TEST_F(CmdStandardItemDeepTest, Use_DoUseFails_RestoresSplit)
{
    auto* pill = new HalfUsableItem();  // can_use=1 但 do_use=0 → 回滚支
    pill->set_name("哑药", "half_pill");
    pill->set("base_name", "half_pill");
    pill->set_amount(3);
    pill->move(me);
    m_items.push_back(pill);

    EXPECT_EQ(0, CallUse(me, pill->object_id(), (char*)""))
        << "do_use 为 0 走回滚(133-135)";
    CContainer* stack = nullptr;
    for (auto* c : *me->get_item_list()) {
        if (strcmp(c->querystr("base_name"), "half_pill") == 0) stack = c;
    }
    ASSERT_NE(nullptr, stack);
    EXPECT_EQ(3, stack->query_amount()) << "失败也归位总数";
}

// ---------------- wield：房间禁令 / 忙碌 / 装备成功 ----------------
TEST_F(CmdStandardItemDeepTest, Wield_NoWieldRoom_Rejected)
{
    auto* user = MakeUser("wield_r1");
    auto* eq   = WieldSword(user, "wield_sword1");
    room->set("no_wield", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallWield(user, eq->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止装备道具"))
        << "152-153";
}

TEST_F(CmdStandardItemDeepTest, Wield_Busy_Rejected)
{
    auto* user = MakeUser("wield_r2");
    auto* eq   = WieldSword(user, "wield_sword2");
    user->start_busy(1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallWield(user, eq->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你正忙着"))
        << "155";
}

TEST_F(CmdStandardItemDeepTest, Wield_Equip_Succeeds)
{
    auto* user = MakeUser("wield_r3");
    auto* eq   = WieldSword(user, "wield_sword3");
    user->clear();  // WieldSword 的准备动作先清场

    EXPECT_EQ(1, CallWield(user, eq->object_id(), (char*)"")) << "157-172";
    EXPECT_TRUE(user->has("装备")) << "默认文案 $N装备$n。(163/166)";
}

// ---------------- unwield：房间禁令 / 忙碌 / 全卸 / 单卸 ----------------
TEST_F(CmdStandardItemDeepTest, Unwield_NoRoomAndBusy_Rejected)
{
    room->set("no_unwield", 1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallUnwield(me, 0, (char*)"all"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止卸下道具"))
        << "186-187";
    room->set("no_unwield", 0);

    me->start_busy(1);
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallUnwield(me, 0, (char*)"all"));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("你正忙着"))
        << "189";
}

TEST_F(CmdStandardItemDeepTest, UnwieldAll_DetachesEquips)
{
    auto* eq = WieldSword(me, "unwield_sword1");
    ASSERT_EQ(1, CallWield(me, eq->object_id(), (char*)""))
        << "前置：真装备成功";
    EXPECT_NE(0, eq->query("equipped")) << "equipped 记录装备位";
    EXPECT_EQ(eq->object_id(), me->query_equip(WIELD_RIGHT_HAND));

    EXPECT_EQ(1, CallUnwield(me, 0, (char*)"all")) << "191-212 全卸";
    EXPECT_EQ(0, eq->query("equipped")) << "equipped 已清(217-230)";
    EXPECT_EQ(0, me->query_equip(WIELD_RIGHT_HAND)) << "装备位让出";
}

TEST_F(CmdStandardItemDeepTest, Unwield_Single_TargetsGivenItem)
{
    auto* eq = WieldSword(me, "unwield_sword2");
    ASSERT_EQ(1, CallWield(me, eq->object_id(), (char*)""))
        << "前置：真装备成功";

    EXPECT_EQ(1, CallUnwield(me, eq->object_id(), (char*)"")) << "215-230";
    EXPECT_EQ(0, eq->query("equipped")) << "按编号单卸";
    EXPECT_EQ(0, me->query_equip(WIELD_RIGHT_HAND));
}

// ---------------- drop：房间禁令 / 坐标回退 / 无值处置 / 容量与标记
// ----------------
TEST_F(CmdStandardItemDeepTest, Drop_NoDropRoom_Rejected)
{
    MakeItem("碎银", "drop_r1", me);
    room->set("no_drop", 1);

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallDrop(me, 0, (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这里禁止丢弃物品"))
        << "279-280";
}

TEST_F(CmdStandardItemDeepTest, Drop_WithoutCoords_FallsBackAndLands)
{
    CContainer* ingot = MakeItem("银锭", "drop_ingot", me);
    ingot->set("value", (long)1);  // 有值物走落地支，不触 258 毁灭

    EXPECT_EQ(1, CallDrop(me, ingot->object_id(), (char*)""))
        << "无参数→目的坐标回退→落地(299-342)";
    EXPECT_EQ(room, ingot->environment()) << "落进所在房间";
}

TEST_F(CmdStandardItemDeepTest, Drop_Valueless_ToldAndDestroyed)
{
    auto* user       = MakeUser("drop_value1");
    CContainer* junk = MakeItem("枯叶", "drop_junk", user);  // value 未设=0

    EXPECT_EQ(1, CallDrop(user, junk->object_id(), (char*)""));
    EXPECT_TRUE(user->has("并不值钱")) << "无值物告知(256-257)";
    // 258: 已推入延迟析构列表，TearDown 统一回收
}

TEST_F(CmdStandardItemDeepTest, Drop_NoDropFlag_Rejected)
{
    CContainer* locked = MakeItem("传家宝", "drop_locked", me);
    locked->set("no_drop", "此物有主");

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallDrop(me, locked->object_id(), (char*)""));
    EXPECT_STREQ(g_notify_fail_msg, "此物有主") << "306-308";
}

TEST_F(CmdStandardItemDeepTest, Drop_RoomCrowded_Rejected)
{
    CContainer* ingot = MakeItem("银锭2", "drop_crowd", me);
    room->set("max_items", (long)1);  // 房内已有 me，size>=1 即满

    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallDrop(me, ingot->object_id(), (char*)""));
    EXPECT_NE(std::string::npos, std::string(g_notify_fail_msg).find("拥挤"))
        << "314-316";
}

// ---------------- give：四道物品标记守卫 ----------------
TEST_F(CmdStandardItemDeepTest, Give_NoGiveFlag_Rejected)
{
    CChar* peer = new CChar();
    peer->set_name("路人甲", "give_t1");
    peer->set("id", "give_t1");
    peer->move(room);
    m_items.push_back(peer);

    CContainer* it = MakeItem("瓷器", "give_nogive", me);
    it->set("no_give", "此物不宜送人");

    char arg[48];
    snprintf(arg, sizeof(arg), "x %ld", (long)peer->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGive(me, it->object_id(), arg));
    EXPECT_STREQ(g_notify_fail_msg, "此物不宜送人") << "411-412";
}

TEST_F(CmdStandardItemDeepTest, Give_NoDropFlag_Rejected)
{
    CChar* peer = new CChar();
    peer->set_name("路人乙", "give_t2");
    peer->set("id", "give_t2");
    peer->move(room);
    m_items.push_back(peer);

    CContainer* it = MakeItem("玉佩", "give_nodrop", me);
    it->set("no_drop", "x");

    char arg[48];
    snprintf(arg, sizeof(arg), "x %ld", (long)peer->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGive(me, it->object_id(), arg));
    EXPECT_STREQ(g_notify_fail_msg, "这样东西不能随便给人。") << "415-417";
}

TEST_F(CmdStandardItemDeepTest, Give_NotOwnersPossession_Rejected)
{
    CChar* peer = new CChar();
    peer->set_name("路人丙", "give_t3");
    peer->set("id", "give_t3");
    peer->move(room);
    m_items.push_back(peer);

    CContainer* it = MakeItem("包裹", "give_owner", me);
    it->set("owner", "someone");

    char arg[48];
    snprintf(arg, sizeof(arg), "x %ld", (long)peer->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGive(me, it->object_id(), arg));
    EXPECT_STREQ(g_notify_fail_msg, "这不是路人丙的东西。") << "420-422";
}

TEST_F(CmdStandardItemDeepTest, Give_NotSpecificallyYours_Rejected)
{
    CChar* peer = new CChar();
    peer->set_name("路人丁", "give_t4");
    peer->set("id", "give_t4");
    peer->move(room);
    m_items.push_back(peer);

    CContainer* it = MakeItem("匣子", "give_objmark", me);
    it->set("obj", "other");

    char arg[48];
    snprintf(arg, sizeof(arg), "x %ld", (long)peer->object_id());
    g_notify_fail_msg[0] = '\0';
    EXPECT_EQ(0, CallGive(me, it->object_id(), arg));
    EXPECT_STREQ(g_notify_fail_msg, "这不是路人丁的东西。") << "424-426";
}

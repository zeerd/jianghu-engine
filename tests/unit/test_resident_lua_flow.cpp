// test_resident_lua_flow.cpp — 居民区 Lua
// 化（lua-default/room/resident_base.lua） 的入住、卖房、清欠与登记回写流程。
//
// 房屋由前缀动态房脚本 room/house_base.lua 承载：house_<属区>_<户主> 存档
// 行由 MockDatabase 提供。
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "test_resident_lua_fixture.h"

using namespace resident_lua_test;

// =====================================================================
// registerhouse：登记入住（起名/名额/资金/落库/属区注册/称号）
// =====================================================================
TEST_F(fixture, RegisterhouseRequiresRoomName)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("age", 20L);

    EXPECT_EQ(1, call_script("registerhouse", {object_arg(&me)}));
    EXPECT_TRUE(me.has("请给您的房间起个名字")) << dump_msgs(me);
    EXPECT_EQ(nullptr, resident_->querymap("house/owner1"));
}

TEST_F(fixture, RegisterhouseRejectsWhenAreaFull)
{
    add_house_entries(70);

    recording_char me;
    me.set("id", "owner1");
    me.set("age", 20L);
    me.set_temp("house/name", "雅室");

    EXPECT_EQ(1, call_script("registerhouse", {object_arg(&me)}));
    EXPECT_TRUE(me.has("本居民区已无空房")) << dump_msgs(me);
}

TEST_F(fixture, RegisterhouseRejectsWithoutFunds)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("age", 20L);
    me.set_temp("house/name", "雅室");

    EXPECT_EQ(1, call_script("registerhouse", {object_arg(&me)}));
    EXPECT_TRUE(me.has("现金不足")) << dump_msgs(me);
    EXPECT_EQ(nullptr, resident_->querymap("house/owner1"));
}

TEST_F(fixture, RegisterhouseSuccessRegistersAndPromotesTitle)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.set("age", 20L);
    me.set("title", "普通百姓");
    me.set_temp("house/name", "雅室");
    me.set_temp("house/desc", "竹影横窗");
    me.set_temp("house/size", 2L);
    me.move(resident_);

    make_coin(&me, 500000);

    EXPECT_EQ(1, call_script("registerhouse", {object_arg(&me)}))
        << dump_msgs(me);
    forget_coin_if_destroyed(&me);
    EXPECT_EQ(nullptr, me.present("coin")) << "现金应被收走";
    EXPECT_EQ('\0', me.querystr_temp("house/name")[0]) << "临时申请数据清空";
    EXPECT_STREQ("测试城", me.querystr("resident"));
    EXPECT_STREQ("测试城百姓", me.querystr("title"));

    // 房屋实例已按申请建好并注册进属区
    ASSERT_NE(nullptr, resident_->querymap("house/owner1")) << dump_msgs(me);
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

TEST_F(fixture, RegisterhouseSavesHouseAfterRegistration)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.set("age", 20L);
    me.set("title", "普通百姓");
    me.set_temp("house/name", "雅室");
    me.set_temp("house/size", 2L);
    me.move(resident_);

    make_coin(&me, 500000);

    EXPECT_EQ(1, call_script("registerhouse", {object_arg(&me)}));
    forget_coin_if_destroyed(&me);
    EXPECT_EQ(nullptr, me.present("coin")) << "现金应被收走";

    // 房屋存档必须晚于注册：long 描述由 update_register 补写，
    // 先存盘会把描述丢掉（房屋进屋后没有场景描述）
    bool saved_with_long = false;
    for (const auto& sql : db_->GetExecutedSqls()) {
        if (sql.find("house_测试城_owner1") == std::string::npos) continue;
        if (sql.find("私人房间") != std::string::npos) saved_with_long = true;
    }
    EXPECT_TRUE(saved_with_long) << "房屋存档应含注册时补写的 long 描述";

    me.move(nullptr);
}

// =====================================================================
// sellhouse：估价/成交/注销
// =====================================================================
TEST_F(fixture, SellhouseNotRegisteredInArea)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("resident", "测试城");

    EXPECT_EQ(1, call_script("sellhouse", {object_arg(&me), text_arg("0")}));
    EXPECT_TRUE(me.has("你要卖掉的房间不在本区！")) << dump_msgs(me);
}

TEST_F(fixture, SellhouseHouseGoneRejected)
{
    register_house_entry("owner1", 0);
    // 不预置房屋存档：get_my_house 拿到 fail_ob 房 → 已被卖掉
    recording_char me;
    me.set("id", "owner1");
    me.set("resident", "测试城");

    EXPECT_EQ(0, call_script("sellhouse", {object_arg(&me), text_arg("0")}));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("已经卖掉"));
}

TEST_F(fixture, SellhouseNotOwnerRejected)
{
    register_house_entry("intruder", 0);
    preset_house_archive("intruder", "owner1", 2);  // 存档户主是别人

    recording_char me;
    me.set("id", "intruder");
    me.set("resident", "测试城");

    EXPECT_EQ(0, call_script("sellhouse", {object_arg(&me), text_arg("0")}));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("这不是你的房子"));
}

TEST_F(fixture, SellhouseQuotesHalfPriceValue)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);

    recording_char me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident_);

    // 房价 50 万，双人房不折半，卖出只值一半 = 25 万；当天卖无税
    EXPECT_EQ(1, call_script("sellhouse", {object_arg(&me), text_arg("0")}))
        << dump_msgs(me);
    EXPECT_TRUE(me.has("目前价值250000"));
    EXPECT_TRUE(me.has("决定卖了"));
    EXPECT_EQ(nullptr, me.present("coin")) << "确认阶段不得付款";
    EXPECT_NE(nullptr, resident_->querymap("house/owner1")) << "未成交不注销";

    me.move(nullptr);
}

TEST_F(fixture, SellhouseCompletePaysAndRemovesHouse)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);

    recording_char me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident_);

    EXPECT_EQ(1, call_script("sellhouse", {object_arg(&me), text_arg("1")}))
        << dump_msgs(me);
    EXPECT_TRUE(me.has("你卖掉了"));

    // 款项到账、注册注销、房屋实例销毁、离线房主临时档回收
    CContainer* coin = me.present("coin");
    ASSERT_NE(nullptr, coin);
    EXPECT_EQ(250000, coin->query_amount());
    items_.push_back(coin);  // 收款进用例物件表，TearDown 统一回收
    EXPECT_EQ(nullptr, resident_->querymap("house/owner1"));
    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "destruct 应按 base_name 摘掉房间";
    EXPECT_EQ(nullptr, find_online("owner1"));

    me.move(nullptr);
}

// =====================================================================
// backhome：进入房屋的权限与在场校验
// =====================================================================
TEST_F(fixture, BackhomeRejectsUnknownHouse)
{
    recording_char me;
    me.set("id", "owner1");

    EXPECT_EQ(1,
              call_script("backhome", {object_arg(&me), text_arg("owner1")}));
    EXPECT_TRUE(me.has("你要进入的房间不在本区！"));
}

TEST_F(fixture, BackhomeRejectsIntruder)
{
    register_house_entry("owner1", 0);

    recording_char me;
    me.set("id", "stranger1");

    EXPECT_EQ(1,
              call_script("backhome", {object_arg(&me), text_arg("owner1")}));
    EXPECT_TRUE(me.has("你试图闯入"));
}

TEST_F(fixture, BackhomeSelfButHouseMissingRejected)
{
    register_house_entry("owner1", 0);
    // 不预置存档：房屋工厂置 fail_ob，load 失败 → 此房间不存在

    recording_char me;
    me.set("id", "owner1");

    EXPECT_EQ(1,
              call_script("backhome", {object_arg(&me), text_arg("owner1")}));
    EXPECT_TRUE(me.has("此房间不存在"));
}

TEST_F(fixture, BackhomeInvitedGuestBlockedWhenMasterAway)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);

    recording_char me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident_);

    EXPECT_EQ(1,
              call_script("backhome", {object_arg(&me), text_arg("owner1")}));
    EXPECT_TRUE(me.has("主人不在家")) << "受邀但主人不在场：不得闯入";

    me.move(nullptr);
}

TEST_F(fixture, BackhomeInvitedGuestEntersWhenMasterHome)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);
    // 房间登记进 g_room_list 由夹具统一回收（先摘仆人再删房）
    CRoom* house_room = load_room("house_测试城_owner1");
    ASSERT_NE(nullptr, house_room);

    // 房内放置"id=owner1"的物件代表主人在家
    CContainer* master_marker = make_item("master_marker");
    master_marker->set("id", "owner1");
    master_marker->move(house_room);

    recording_char me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident_);

    EXPECT_EQ(1, call_script("backhome", {object_arg(&me), text_arg("owner1")}))
        << dump_msgs(me);
    ASSERT_NE(nullptr, me.environment()) << "受邀客人应已入房";
    EXPECT_EQ(static_cast<CContainer*>(house_room), me.environment())
        << "受邀客人移入房屋";

    me.move(nullptr);
    master_marker->move(nullptr);
}

// =====================================================================
// clear_house：欠费扣除（不足 30 天不动、扣款、余额不足）
// =====================================================================
TEST_F(fixture, ClearHouseNoRegisteredHousesDoesNothing)
{
    const size_t before = db_->GetExecutedSqls().size();
    call_script("clear_house");
    EXPECT_EQ(before, db_->GetExecutedSqls().size()) << "无登记时不产生库操作";
}

TEST_F(fixture, ClearHouseRecentDebtIsNotTouched)
{
    register_house_entry("owner1", 5);
    call_script("clear_house");

    EXPECT_NE(nullptr, resident_->querymap("house/owner1"));
    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "未到期不触碰房屋实例";
}

TEST_F(fixture, ClearHouseDeductsRentFromOnlineBalance)
{
    preset_house_archive("owner1", "owner1", 2);
    register_house_entry("owner1", 31);
    ScopedOnlineUser owner("owner1", "房主");
    owner.get()->set("balance", 10000000L);

    call_script("clear_house");

    // 欠 31 天 × 50 万 / 250 = 6.2 万，银行余额扣清后房主保有房产
    CUser* u = find_online("owner1");
    ASSERT_NE(nullptr, u) << "在线房主不被销毁";
    EXPECT_EQ(10000000 - 62000, u->query("balance"));

    auto it = g_room_list.find("house_测试城_owner1");
    ASSERT_NE(g_room_list.end(), it);
    EXPECT_EQ(g_current_time, it->second->query("pay_time")) << "付费时间刷新";
    EXPECT_NE('\0', it->second->querystr("long")[0])
        << "注册回写更新了房间描述";
    EXPECT_NE(nullptr, resident_->querymap("house/owner1"))
        << "扣款成功：注册保留";
}

TEST_F(fixture, ClearHouseSmallHouseHalvesRent)
{
    preset_house_archive("owner1", "owner1", 1);  // 单人房租金减半
    register_house_entry("owner1", 31);
    ScopedOnlineUser owner("owner1", "房主");
    owner.get()->set("balance", 100L);

    call_script("clear_house");

    // 半租 3.1 万：余额 100 扣成负数且不足付清 → 只扣款不收房
    CUser* u = find_online("owner1");
    ASSERT_NE(nullptr, u);
    EXPECT_EQ(100 - 31000, u->query("balance"));
    EXPECT_NE(nullptr, resident_->querymap("house/owner1"));
}

TEST_F(fixture, ClearHouseOfflineOwnerReclaimedAfterRentPass)
{
    preset_house_archive("owner1", "owner1", 2);
    register_house_entry("owner1", 31);

    call_script("clear_house");

    // 离线房主被临时装载、扣款后销毁回收
    EXPECT_EQ(nullptr, find_online("owner1"));
    EXPECT_NE(nullptr, resident_->querymap("house/owner1"));
}

TEST_F(fixture, ClearHouseMissingRoomCancelsRegistration)
{
    // 欠费户的房屋档案已不存在：注销登记并返回
    register_house_entry("owner1", 31);
    call_script("clear_house");

    EXPECT_EQ(nullptr, resident_->querymap("house/owner1"));
}

// =====================================================================
// remove_house / update_register：注销并销毁房屋、登记回写
// =====================================================================
TEST_F(fixture, RemoveHouseClearsOwnerAndDestroysRoom)
{
    register_house_entry("owner1", 0);
    CLuaRoom* house = new CLuaRoom("house_base");
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    g_room_list["house_测试城_owner1"] = house;

    call_script("remove_house", {object_arg(house)});

    EXPECT_EQ(g_room_list.end(), g_room_list.find("house_测试城_owner1"))
        << "房屋从房间表摘除";
    EXPECT_EQ(nullptr, resident_->querymap("house/owner1")) << "注册注销";
    EXPECT_EQ(nullptr, find_online("owner1")) << "离线房主装载后销毁";
    EXPECT_TRUE(db_->ExecutedContaining("delete from t_room"))
        << "房屋档案从库中删除";
}

TEST_F(fixture, UpdateRegisterOfflineOwnersLoadAndDestroy)
{
    CLuaRoom* house = new CLuaRoom("house_base");
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    house->set("second_owner", "wife1");
    house->set("pay_time", static_cast<int32_t>(k_now));

    call_script("update_register", {object_arg(house)});

    ASSERT_NE(nullptr, resident_->querymap("house/owner1"));
    EXPECT_STREQ("wife1",
                 resident_->querymap("house/owner1")->querystr("owner"));
    EXPECT_EQ(nullptr, find_online("owner1")) << "离线第一房主装载后销毁";
    EXPECT_EQ(nullptr, find_online("wife1")) << "离线第二房主装载后销毁";
    EXPECT_NE(std::string::npos,
              std::string(house->querystr("long")).find("共享房间"));

    delete house;
}

TEST_F(fixture, UpdateRegisterOfflineSingleOwner)
{
    CLuaRoom* house = new CLuaRoom("house_base");
    house->set("base_name", "house_测试城_owner1");
    house->set_name("雅室");
    house->set("owner", "owner1");
    house->set("pay_time", static_cast<int32_t>(k_now));

    call_script("update_register", {object_arg(house)});

    ASSERT_NE(nullptr, resident_->querymap("house/owner1"));
    EXPECT_EQ('\0', resident_->querymap("house/owner1")->querystr("owner")[0])
        << "无第二房主时注册 owner 为空";
    EXPECT_EQ(nullptr, find_online("owner1"));
    EXPECT_NE(std::string::npos,
              std::string(house->querystr("long")).find("私人房间"));

    delete house;
}

// =====================================================================
// setup / reset：维护窗口倒计时到期后执行载入与清欠
// =====================================================================
TEST_F(fixture, SetupTriggersArchiveLoad)
{
    resident_->setup();
    EXPECT_TRUE(db_->ExecutedContaining("resident_test_a"))
        << "setup 应触发一次属区档案装载";
}

TEST_F(fixture, ResetCountsDownThenRunsMaintenance)
{
    // 欠 31 天且无房屋存档：维护窗口到期后 clear_house 注销该户
    register_house_entry("owner1", 31);
    resident_->setup();

    // wait_clear ∈ [40,79]：80 次 reset 必然走完倒计时
    for (int i = 0; i < 80; i++) {
        resident_->reset();
    }
    EXPECT_EQ(nullptr, resident_->querymap("house/owner1"))
        << "到期维护执行了 clear_house（欠费户被注销）";
}

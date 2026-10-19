// test_resident_lua.cpp — 居民区 Lua 化（lua-default/room/resident_base.lua）
// 的查询、菜单与命令分发契约。
//
// 属区脚本 fixture_resident_room.lua（显式 install）与
// fixture_resident_auto.lua（type = "resident" 由引擎自动装配）承载；
// 房屋由前缀动态房脚本 room/house_base.lua 承载。
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "test_resident_lua_fixture.h"

using namespace resident_lua_test;

// =====================================================================
// 脚本声明的属性 / 契约查询：CAN_SAVE / IS_RESIDENT / 地价 / 欠费天数
// =====================================================================
TEST_F(fixture, ScriptDeclaresSaveAndResidentAndRunsCreate)
{
    EXPECT_EQ(1, resident_->query_attr(CMainObject::CAN_SAVE));
    EXPECT_EQ(1, resident_->query_attr(CMainObject::IS_RESIDENT));
    EXPECT_EQ(1, resident_->query_attr(CMainObject::IS_ROOM));

    EXPECT_STREQ("测试住宅区", resident_->name());
    EXPECT_STREQ("测试城", resident_->querystr("resident"));
    EXPECT_STREQ("沿街是一排青砖小院。", resident_->querystr("long"));
}

TEST_F(fixture, InstallKeepsScriptOverrides)
{
    CLuaRoom room("fixture_resident_override");
    room.create();

    EXPECT_EQ(1, room.query_attr(CMainObject::IS_RESIDENT));

    // 脚本自带 do_look 时 install 不覆盖：房间钩子分派直接落到脚本版本
    recording_char me;
    me.set("id", "stranger1");
    EXPECT_EQ(1, room.do_look(&me));
    EXPECT_TRUE(me.has("脚本自带的视野")) << dump_msgs(me);
}

// =====================================================================
// 自动装配：type = "resident" 的房间脚本无需 require + install
// =====================================================================
TEST_F(fixture, ResidentTypeRoomGetsMixinWithoutInstall)
{
    // 脚本只声明 type = "resident"，不调 install：引擎在加载期装配
    CLuaRoom room("fixture_resident_auto");
    room.create();

    EXPECT_EQ(1, room.query_attr(CMainObject::CAN_SAVE));
    EXPECT_EQ(1, room.query_attr(CMainObject::IS_RESIDENT));
    EXPECT_STREQ("装配城", room.querystr("resident"));
    EXPECT_STREQ("新砌的粉墙围着一排小院。", room.querystr("long"));

    // 属区行为随装配生效：do_look 出现本区申请菜单
    recording_char me;
    me.set("id", "stranger1");
    me.move(&room);
    EXPECT_EQ(1, room.do_look(&me));
    EXPECT_TRUE(me.has("申请成为装配城居民")) << dump_msgs(me);
    me.move(nullptr);
}

TEST_F(fixture, PlainRoomTypeSkipsMixin)
{
    // type ~= "resident" 的普通房间不装配：resident_area 只由 install 设置
    CLuaRoom room("fixture_blank_room");
    room.create();

    EXPECT_EQ(0, room.query_attr(CMainObject::IS_RESIDENT));
    EXPECT_EQ(1, room.query_attr(CMainObject::IS_ROOM));
}

TEST_F(fixture, ResidentTypeRoomRemixinOnReload)
{
    // 热重载与首载走同一装配入口：重载出的全新脚本表同样带上属区行为
    //（若重载路径漏装，resident_area 缺席 → 本断言为 false）
    CLuaEngine* engine = CLuaEngine::instance();
    EXPECT_NE(LUA_NOREF, engine->reload_script_by_name("fixture_resident_auto"));
    EXPECT_TRUE(engine->get_script_bool_field("fixture_resident_auto",
                                              LUA_SCRIPT_ROOM, "resident_area"))
        << "重载后的脚本表未被装配";
}

TEST_F(fixture, ResidentMixinFailureDoesNotBreakRoomLoad)
{
    // 真实故障场景：require 不可用时装配失败只记日志，房间仍可加载
    // 过程：摘 require → 重载 resident 房间 → 表加载成功但未装配
    //       → 恢复 require → 再次重载 → 装配恢复
    CLuaEngine* engine = CLuaEngine::instance();
    lua_State* L = engine->get_state();

    // 1) 保存原 require
    lua_getglobal(L, "require");
    int saved_require_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // 2) 摘 require
    lua_pushnil(L);
    lua_setglobal(L, "require");

    // 3) 触发重载（走装配 helper）
    EXPECT_NE(LUA_NOREF, engine->reload_script_by_name("fixture_resident_auto"));

    // 4) 装配失败 → resident_area 为 false（错误分支被执行，覆盖率入账）
    EXPECT_FALSE(engine->get_script_bool_field("fixture_resident_auto",
                                               LUA_SCRIPT_ROOM, "resident_area"))
        << "装配失败应导致 resident_area 缺席";

    // 5) 恢复 require
    lua_rawgeti(L, LUA_REGISTRYINDEX, saved_require_ref);
    lua_setglobal(L, "require");
    luaL_unref(L, LUA_REGISTRYINDEX, saved_require_ref);

    // 6) 再次重载 → 装配恢复
    EXPECT_NE(LUA_NOREF, engine->reload_script_by_name("fixture_resident_auto"));
    EXPECT_TRUE(engine->get_script_bool_field("fixture_resident_auto",
                                              LUA_SCRIPT_ROOM, "resident_area"))
        << "恢复 require 后装配应重新生效";
}

TEST_F(fixture, OverdueDaysFromRegisterTime)
{
    EXPECT_EQ(0, call_script("count_days", {text_arg("owner1")}));

    register_house_entry("owner1", 3);
    EXPECT_EQ(3, call_script("count_days", {text_arg("owner1")}));
    EXPECT_EQ(0, call_script("count_days", {text_arg("ghost")}));
}

TEST_F(fixture, HousePriceScalesWithHouseCount)
{
    EXPECT_EQ(500000, call_script("house_price"));

    // 户数/5 定价档：5 户 → 档 1
    add_house_entries(5);
    EXPECT_EQ(600000, call_script("house_price"));

    // 超档封顶：75 户 → 档 15 > 14 → 1300 万
    add_house_entries(70);
    EXPECT_EQ(13000000, call_script("house_price"));
}

TEST_F(fixture, HousePriceHonorsPolicyFixedPrice)
{
    // 属区声明固定地价时按声明值（数据集自定地价，缺省阶梯价不生效）
    resident_->set("policy/house_price", 5000L);
    EXPECT_EQ(5000, call_script("house_price"));

    // 声明值不随住户数变化
    add_house_entries(10);
    EXPECT_EQ(5000, call_script("house_price"));
}

TEST_F(fixture, HouseMinAgeDefaultsAndPolicy)
{
    EXPECT_EQ(16, call_script("house_min_age"));

    resident_->set("policy/min_age", 14L);
    EXPECT_EQ(14, call_script("house_min_age"));
}

TEST_F(fixture, CanApplyHouseCountLimit)
{
    EXPECT_EQ(1, call_script("can_apply_house"));

    add_house_entries(69);
    EXPECT_EQ(1, call_script("can_apply_house")) << "未满 70 户仍可申请";

    add_house_entries(1);
    EXPECT_EQ(0, call_script("can_apply_house")) << "满 70 户停止申请";
}

TEST_F(fixture, CanApplyHouseHonorsCapacityDecl)
{
    // 属区在 dbase 声明 capacity 即生效（未声明走缺省 70，见上例）
    resident_->set("capacity", 2L);
    add_house_entries(2);
    EXPECT_EQ(0, call_script("can_apply_house")) << "满 capacity 停止申请";

    resident_->set("capacity", 5L);
    EXPECT_EQ(1, call_script("can_apply_house")) << "提高 capacity 后恢复申请";
}

// =====================================================================
// do_look：住户/非住户/邀请三条视线分支
// =====================================================================
TEST_F(fixture, DoLookNoHousesShowsApplyMenu)
{
    recording_char me;
    me.set("id", "stranger1");
    me.move(resident_);

    EXPECT_EQ(1, resident_->do_look(&me));
    EXPECT_TRUE(me.has("没有任何住户")) << dump_msgs(me);
    EXPECT_TRUE(me.has("申请成为测试城居民"));
    EXPECT_FALSE(me.has("回家"));
    EXPECT_FALSE(me.has("查看有哪些住户"));

    me.move(nullptr);
}

TEST_F(fixture, DoLookResidentSeesHomeAndSellMenus)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);

    recording_char me;
    me.set("id", "owner1");
    me.set("resident", "测试城");
    me.move(resident_);

    EXPECT_EQ(1, resident_->do_look(&me));
    EXPECT_TRUE(me.has("住宅区。目前这里有1家住户")) << dump_msgs(me);
    EXPECT_TRUE(me.has("回家"));
    EXPECT_TRUE(me.has("查看有哪些住户"));
    EXPECT_TRUE(me.has("卖掉")) << "名下有房时出现卖房入口";
    EXPECT_FALSE(me.has("申请成为")) << "本区住户不再见申请入口";

    me.move(nullptr);
}

TEST_F(fixture, DoLookInviteOffersVisitMenu)
{
    register_house_entry("owner1", 0);
    preset_house_archive("owner1", "owner1", 2);
    ScopedOnlineUser online("owner1", "房主");

    recording_char me;
    me.set("id", "stranger1");
    me.set_temp("invite", "owner1");
    me.move(resident_);

    EXPECT_EQ(1, resident_->do_look(&me));
    EXPECT_TRUE(me.has("去房主的家")) << dump_msgs(me);
    EXPECT_TRUE(me.has("$0enter 0 owner1"));

    me.move(nullptr);
}

TEST_F(fixture, DoLookShowsScriptLongBeforeSummary)
{
    // 房间脚本声明的 long 是本区场景描述：先于引擎的住户统计出现
    recording_char me;
    me.set("id", "stranger1");
    me.move(resident_);

    EXPECT_EQ(1, resident_->do_look(&me));
    ASSERT_GE(me.msgs.size(), 2u) << dump_msgs(me);
    EXPECT_NE(std::string::npos, me.msgs[0].find("沿街是一排青砖小院。"))
        << dump_msgs(me);
    EXPECT_NE(std::string::npos, me.msgs[1].find("住宅区。目前这里"))
        << dump_msgs(me);

    me.move(nullptr);
}

// =====================================================================
// applyhouse / need_money / applyroom / menulist：购房申请流
// =====================================================================
TEST_F(fixture, ApplyhouseGuardsAndMenu)
{
    recording_char member;
    member.set("id", "owner1");
    member.set("resident", "测试城");
    EXPECT_EQ(1, call_script("applyhouse", {object_arg(&member)}));
    EXPECT_TRUE(member.has("你已经申请成为")) << dump_msgs(member);

    recording_char minor;
    minor.set("id", "kid1");
    minor.set("age", 15L);
    EXPECT_EQ(1, call_script("applyhouse", {object_arg(&minor)}));
    EXPECT_TRUE(minor.has("你还没成年"));

    recording_char adult;
    adult.set("id", "adult1");
    adult.set("age", 20L);
    EXPECT_EQ(1, call_script("applyhouse", {object_arg(&adult)}));
    EXPECT_TRUE(adult.has("申请单人间"));
    EXPECT_TRUE(adult.has("申请双人套房"));
}

TEST_F(fixture, ApplyhouseHonorsPolicyMinAge)
{
    // 属区声明 policy/min_age 后，未成年也准予置产（数据集自定门槛）
    resident_->set("policy/min_age", 14L);

    recording_char teen;
    teen.set("id", "teen1");
    teen.set("age", 14L);
    EXPECT_EQ(1, call_script("applyhouse", {object_arg(&teen)}));
    EXPECT_TRUE(teen.has("申请单人间"));
    EXPECT_FALSE(teen.has("你还没成年"));

    recording_char kid;
    kid.set("id", "kid1");
    kid.set("age", 13L);
    EXPECT_EQ(1, call_script("applyhouse", {object_arg(&kid)}));
    EXPECT_TRUE(kid.has("你还没成年"));
}

TEST_F(fixture, NeedMoneyHalvesForSingleRoom)
{
    recording_char me;
    me.set("id", "owner1");
    EXPECT_EQ(500000, call_script("need_money", {object_arg(&me)}));

    me.set_temp("house/size", 1);
    EXPECT_EQ(250000, call_script("need_money", {object_arg(&me)}))
        << "单人房半价";
}

TEST_F(fixture, ApplyroomRequiresEnoughCoin)
{
    recording_char me;
    me.set("id", "owner1");

    // 无现金：按全价提示退回
    EXPECT_EQ(1, call_script("applyroom", {object_arg(&me), text_arg("2")}));
    EXPECT_TRUE(me.has("申请房间需要500000金")) << dump_msgs(me);

    // 现金不足：同样退回
    CContainer* coin = make_item("coin");
    coin->set("value", 100L);
    coin->move(&me);
    me.clear();
    EXPECT_EQ(1, call_script("applyroom", {object_arg(&me), text_arg("2")}));
    EXPECT_TRUE(me.has("申请房间需要500000金"));

    // 单人房折半后现金足够 → 进入房间名设置菜单
    coin->set("value", 250000L);
    me.clear();
    EXPECT_EQ(1, call_script("applyroom", {object_arg(&me), text_arg("1")}));
    EXPECT_TRUE(me.has("设置房间名称"));
    EXPECT_TRUE(me.has("设置房间描述"));
    EXPECT_FALSE(me.has("设置第二房主")) << "单人房不提供第二房主";

    me.move(nullptr);
}

TEST_F(fixture, MenulistAdvertsSecondOwnerAndApply)
{
    recording_char me;
    me.set("id", "owner1");

    call_script("menulist", {object_arg(&me)});
    EXPECT_TRUE(me.has("设置房间名称"));
    EXPECT_TRUE(me.has("设置房间描述"));
    EXPECT_FALSE(me.has("设置第二房主"));
    EXPECT_FALSE(me.has("申请房间")) << "未起名时不出现登记入口";

    me.clear();
    me.set_temp("house/size", 2);
    me.set_temp("house/name", "雅室");
    call_script("menulist", {object_arg(&me)});
    EXPECT_TRUE(me.has("设置第二房主")) << "双人房可设第二房主";
    EXPECT_TRUE(me.has("申请房间")) << "起名后出现登记入口";
}

// =====================================================================
// housename / housedesc / addmaster：房间属性设置（命令经 UTF-8 入脚本）
// =====================================================================
TEST_F(fixture, HousenameRejectsIllegalAndSetsTemp)
{
    recording_char me;
    me.set("id", "owner1");

    EXPECT_EQ(1, resident_->handle_action((char*)"housename", &me, (char*)"a",
                                          (char*)""));
    EXPECT_TRUE(me.has("对不起")) << dump_msgs(me);
    EXPECT_EQ('\0', me.querystr_temp("house/name")[0]);

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"housename", &me,
                                          (char*)"雅室", (char*)""));
    EXPECT_STREQ("雅室", me.querystr_temp("house/name")) << dump_msgs(me);
}

TEST_F(fixture, HousedescGuardsLength)
{
    recording_char me;
    me.set("id", "owner1");

    EXPECT_EQ(1, resident_->handle_action((char*)"housedesc", &me, (char*)"",
                                          (char*)""));
    EXPECT_TRUE(me.has("简洁漂亮"));
    EXPECT_EQ('\0', me.querystr_temp("house/desc")[0]);

    std::string long_desc(501, 'x');
    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"housedesc", &me,
                                          &long_desc[0], (char*)""));
    EXPECT_TRUE(me.has("简洁漂亮"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"housedesc", &me,
                                          (char*)"竹影横窗", (char*)""));
    EXPECT_STREQ("竹影横窗", me.querystr_temp("house/desc"));
}

TEST_F(fixture, AddmasterRequiresTargetOnline)
{
    recording_char me;
    me.set("id", "owner1");

    EXPECT_EQ(1,
              call_script("addmaster", {object_arg(&me), text_arg("ghost1")}));
    EXPECT_TRUE(me.has("请这个人在线时"));
    EXPECT_EQ('\0', me.querystr_temp("house/owner")[0]);

    ScopedOnlineUser friend_online("friend1", "好友");
    me.clear();
    EXPECT_EQ(1,
              call_script("addmaster", {object_arg(&me), text_arg("friend1")}));
    EXPECT_STREQ("friend1", me.querystr_temp("house/owner"));
}

// =====================================================================
// check_second_owner / checkhouse：住户名录
// =====================================================================
TEST_F(fixture, CheckSecondOwnerOffersEnterMenuForOthersHome)
{
    // house/<户主> 映射的 owner 字段 = 第二房主（生产 update_register 语义）
    register_house_entry("owner1", 0, "雅室", "房主", "wife1");

    recording_char wife;
    wife.set("id", "wife1");
    call_script("check_second_owner", {object_arg(&wife)});
    EXPECT_TRUE(wife.has("进入雅室"));
    EXPECT_TRUE(wife.has("$0enter 0 owner1"));

    recording_char stranger;
    stranger.set("id", "stranger1");
    call_script("check_second_owner", {object_arg(&stranger)});
    EXPECT_FALSE(stranger.has("进入雅室")) << "非第二房主不出现他人房产入口";
}

TEST_F(fixture, CheckhouseListsRegisteredHomes)
{
    recording_char me;
    me.set("id", "owner1");
    EXPECT_EQ(0, call_script("checkhouse", {object_arg(&me)}))
        << "无住户返回 0";

    register_house_entry("owner1", 0, "雅室", "房主");
    me.clear();
    EXPECT_EQ(1, call_script("checkhouse", {object_arg(&me)}));
    EXPECT_TRUE(me.has("雅室(房主)"));
    EXPECT_TRUE(me.has("$0enter 0 owner1"));
}

// =====================================================================
// handle_action：房管命令分发（每条命令落进对应处理函数）
// =====================================================================
TEST_F(fixture, HandleActionRoutesEveryHouseCommand)
{
    recording_char me;
    me.set("id", "owner1");
    me.set("name", "房主");
    me.set("age", 20L);
    me.set("resident", "测试城");
    me.move(resident_);

    EXPECT_EQ(1, resident_->handle_action((char*)"enter", &me, (char*)"owner1",
                                          (char*)""));
    EXPECT_TRUE(me.has("你要进入的房间不在本区！")) << dump_msgs(me);

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"applyhouse", &me, (char*)"",
                                          (char*)""));
    EXPECT_TRUE(me.has("你已经申请成为"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"applyroom", &me, (char*)"2",
                                          (char*)""));
    EXPECT_TRUE(me.has("申请房间需要"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"housename", &me, (char*)"a",
                                          (char*)""));
    EXPECT_TRUE(me.has("对不起"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"housedesc", &me, (char*)"",
                                          (char*)""));
    EXPECT_TRUE(me.has("简洁漂亮"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"registerhouse", &me,
                                          (char*)"", (char*)""));
    EXPECT_TRUE(me.has("请给您的房间起个名字"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"addmaster", &me,
                                          (char*)"ghost1", (char*)""));
    EXPECT_TRUE(me.has("请这个人在线时"));

    me.clear();
    EXPECT_EQ(0, resident_->handle_action((char*)"checkhouse", &me, (char*)"",
                                          (char*)""));
    EXPECT_FALSE(me.has("查看有哪些住户"));

    me.clear();
    EXPECT_EQ(1, resident_->handle_action((char*)"sellhouse", &me, (char*)"0",
                                          (char*)""));
    EXPECT_TRUE(me.has("你要卖掉的房间不在本区！"));

    me.clear();
    EXPECT_EQ(0, resident_->handle_action((char*)"unknown_command", &me,
                                          (char*)"", (char*)""));
    EXPECT_FALSE(me.has("房间")) << "未知命令落回 CRoom::handle_action";

    me.move(nullptr);
}

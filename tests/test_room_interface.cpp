// test_room_interface.cpp - CRoom 的接口实现（RoomInterface.cpp）

//

// 锁定三块可观察契约：

//   1) IDialogSource::say —— me 为空时以房间口吻广播（chat 通道投递房内玩家）；

//   2) IClientObject::download —— 向观看者下行 DOWNOBJECT 封包；

//   3) get_info —— 房间信息串拼装：基础字段 + locked 状态位 + 标题位。

//

// Build+Run: .\run-coverage.ps1

#include "pch.h"

#include <string>

#include "Global.h"

#include "Room.h"

#include "Mapping.h"  // corps 映射构造

#include "User.h"

#include "test_lua_env.h"

#include "test_scoped_room.h"

#include "test_state_recorder.h"

class RoomInterfaceTest : public ::testing::Test {
protected:
    std::unique_ptr<ScopedRoom> room;

    StateRecordingUser* viewer;

    void SetUp() override

    {
        ASSERT_NE(ensure_lua_env(), nullptr);

        room = std::make_unique<ScopedRoom>("pk");

        ASSERT_NE(room->get(), nullptr);

        viewer = new StateRecordingUser();

        viewer->set_name("看客", "iface_viewer");

        viewer->set("level", (long)10);

        viewer->move(room->get());

        viewer->delete_temp("client/loading");
    }

    void TearDown() override

    {
        viewer->move(nullptr);

        delete viewer;

        room.reset();
    }
};

// ---------------- say：房间口吻广播到发言者所在容器的成员 ----------------

TEST_F(RoomInterfaceTest, Say_BroadcastsChatToRoomUsers)

{
    // message_vision_room(chat) 取发言者 environment 的成员作为收听面：

    // 顶层层房间无 environment 会早退，故套一层区域容器、观众同级放置

    auto* region = new CContainer();

    region->set_name("大千世界", "iface_region");

    room->get()->move(region);

    viewer->move(region);

    int rc = room->get()->say("劳驾让让路", nullptr, nullptr);

    EXPECT_EQ(1, rc);

    EXPECT_TRUE(viewer->has("说：")) << "房间口吻前缀(53)";

    EXPECT_TRUE(viewer->has("劳驾让让路")) << "原话入包(53)";

    viewer->move(nullptr);

    room->get()->move(nullptr);

    delete region;
}

// ---------------- download：DOWNOBJECT 封包与 locked 状态位 ----------------

TEST_F(RoomInterfaceTest, Download_LockedRoom_CarriesLockField)

{
    room->get()->set("status", "locked");

    room->get()->set("reason", "维修中");

    room->get()->download(viewer);

    EXPECT_TRUE(viewer->has("O=")) << "DOWNOBJECT 封包头(73)";

    EXPECT_TRUE(viewer->has("N=")) << "基础字段(83)";

    EXPECT_TRUE(viewer->has("l=维修中")) << "locked 状态位(87-88)";

    EXPECT_TRUE(viewer->has("t=")) << "标题位(91)";
}

TEST_F(RoomInterfaceTest, Download_UnlockedRoom_HasNoLockField)

{
    room->get()->download(viewer);

    EXPECT_TRUE(viewer->has("t=")) << "非 locked 也带标题位(91)";

    EXPECT_FALSE(viewer->has("l=")) << "未锁不带状态位(87 短路)";
}

// ---------------- Room.cpp: handle_action("back") 认尸返魂跳转(341-358)

// ----------------

TEST_F(RoomInterfaceTest, HandleActionBackJumpsToCorpseRoom)

{
    // 尸房登记：corps={pk:1} —— 认尸后跳回 pk

    CMapping corps;

    corps.set("pk", (long)1);

    room->get()->set("corps", corps);

    CChar me;

    me.set_name("返魂者", "back_me");

    me.set("corps/id", "pk");

    CContainer* stage = new CContainer();

    stage->set_name("停尸间", "back_stage");

    me.move(stage);

    char comm[] = "back";

    char arg[] = "";

    int rc = room->get()->handle_action(comm, &me, arg, nullptr);

    EXPECT_EQ(1, rc) << "认尸成功返回(358)";

    EXPECT_NE(stage, me.environment()) << "已跳离原地(355)";

    EXPECT_EQ(nullptr, room->get()->querymap("corps")->querymap("nonexistent"))

        << "corps 映射仍可查(占位观察)";

    me.move(nullptr);

    delete stage;
}

// ---------------- ContainerItem.cpp: query_short 两支(62-64) ----------------
TEST_F(RoomInterfaceTest, QueryShortPrefersShortDesc)
{
    auto* r = room->get();
    EXPECT_STREQ(r->name(), r->query_short()) << "无 short → 回落 name(64)";

    r->set("short", "一间暗室");
    EXPECT_STREQ("一间暗室", r->query_short()) << "有 short → 短描述(62-63)";
    r->del("short");
}

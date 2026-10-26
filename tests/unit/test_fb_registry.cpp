// test_fb_registry.cpp - Unit tests for CFBRegistry room.
//
#include "pch.h"
#include <memory>
#include <string>
#include "Char.h"
#include "Room.h"
#include "FBRegistry.h"

// create_room 定义在 src/engine/SetupObjects.cpp（未进任何头文件，
// RoomMgr.cpp 也是本地声明），这里按同签名声明以便直测工厂分支。
std::unique_ptr<CRoom> create_room(std::string room_name);

// ---------------------------------------------------------------------------
// Test: CFBRegistry blocks get/drop/give/use
// ---------------------------------------------------------------------------
TEST(FBRegistry, BlocksGet)
{
    CFBRegistry room;
    room.create();
    EXPECT_EQ(room.handle_action((char*)"get", nullptr, (char*)"", (char*)""),
              1);
}

TEST(FBRegistry, BlocksDrop)
{
    CFBRegistry room;
    room.create();
    EXPECT_EQ(room.handle_action((char*)"drop", nullptr, (char*)"", (char*)""),
              1);
}

TEST(FBRegistry, BlocksGive)
{
    CFBRegistry room;
    room.create();
    EXPECT_EQ(room.handle_action((char*)"give", nullptr, (char*)"", (char*)""),
              1);
}

TEST(FBRegistry, BlocksUse)
{
    CFBRegistry room;
    room.create();
    EXPECT_EQ(room.handle_action((char*)"use", nullptr, (char*)"", (char*)""),
              1);
}

// ---------------------------------------------------------------------------
// Test: 非物品指令回落 CRoom::handle_action（分支其余指令交回房间处理）
// ---------------------------------------------------------------------------
TEST(FBRegistry, NonItemCommandDelegatesToRoom)
{
    CFBRegistry room;
    room.create();
    EXPECT_EQ(room.handle_action((char*)"look", nullptr, (char*)"", (char*)""),
              0)
        << "非 get/drop/give/use 交回房间处理(22)";
}

// ---------------------------------------------------------------------------
// Test: 内置副本注册处不读库——t_room 里没有 fb_registry 这一行，
// 兜底分支若照常 load() 必然失败，既误报"没有该房间"，又给它挂上 fail_ob
// ---------------------------------------------------------------------------
TEST(FBRegistry, CreateRoomSkipsDbLoadForBuiltinRoom)
{
    std::unique_ptr<CRoom> room = create_room("fb_registry");

    ASSERT_NE(room, nullptr);
    EXPECT_NE(dynamic_cast<CFBRegistry*>(room.get()), nullptr)
        << "兜底分支应产出 CFBRegistry(0)";
    EXPECT_EQ(room->query("fail_ob"), 0)
        << "内置房无存盘行，不该被标成失败房(0)";
    EXPECT_EQ(room->query("infinity"), 1) << "兜底分支仍标记 infinity(1)";
}

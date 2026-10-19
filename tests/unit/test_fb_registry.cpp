// test_fb_registry.cpp - Unit tests for CFBRegistry room.
//
#include "pch.h"
#include <string>
#include "Char.h"
#include "Room.h"
#include "FBRegistry.h"

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

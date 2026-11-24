// cli/tests/room_map_test.cc — 房间地图解析与寻路
//
// Build+Run（仓库根目录）：
//   msbuild cli\tests\jianghu_cli_tests.vcxproj /p:Configuration=Release
//     /p:Platform=x64
//   bin\cli\x64\jianghu_cli_tests.exe --gtest_filter=RoomMap*

#include "room_map.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "test_tmp_dir.h"

TEST(RoomMapTest, LoadsSampleMapAndFindsNames)
{
    RoomMap map;
    ASSERT_TRUE(map.load("cli/sample.dat"));
    EXPECT_EQ(map.size(), 10u);
    EXPECT_EQ(map.find_room("青云山山脚"), 0);
    EXPECT_EQ(map.find_room("山脚钱庄"), 4);  // Name1
    // 显示名（Name2 / set_name 短名）不参与定位：全服允许重名
    EXPECT_EQ(map.find_room("钱庄"), -1);
    EXPECT_EQ(map.find_room("不存在的房间"), -1);
}

TEST(RoomMapTest, MissingFileFailsToLoad)
{
    RoomMap map;
    EXPECT_FALSE(map.load("no_such_map_file.dat"));
    EXPECT_TRUE(map.empty());
}

TEST(RoomMapTest, FindsDirectStep)
{
    RoomMap map;
    ASSERT_TRUE(map.load("cli/sample.dat"));

    auto steps = map.find_path("青云山山脚", "山脚茶棚");
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0].go_key, "山脚茶棚");
    EXPECT_EQ(steps[0].arrival_name, "山脚茶棚");
}

TEST(RoomMapTest, FindsMultiStepPath)
{
    RoomMap map;
    ASSERT_TRUE(map.load("cli/sample.dat"));

    auto steps = map.find_path("青云山山顶", "山脚茶棚");
    ASSERT_EQ(steps.size(), 3u);
    EXPECT_EQ(steps[0].go_key, "青云山半山腰");
    EXPECT_EQ(steps[1].go_key, "青云山山脚");
    EXPECT_EQ(steps[2].go_key, "山脚茶棚");
    // 终点是 Name2 与 Name1 不同的房间，到房判定仍走注册名
    auto to_bank = map.find_path("青云山山脚", "山脚钱庄");
    ASSERT_EQ(to_bank.size(), 1u);
    EXPECT_EQ(to_bank[0].go_key, "山脚钱庄");
    EXPECT_EQ(to_bank[0].arrival_name, "山脚钱庄");
}

TEST(RoomMapTest, SameAndDisconnectedRoomsHaveNoPath)
{
    RoomMap map;
    ASSERT_TRUE(map.load("cli/sample.dat"));

    EXPECT_TRUE(map.find_path("青云山山脚", "青云山山脚").empty());
    // 副本房间只能通过山顶菜单进入，没有连通的门
    EXPECT_TRUE(map.find_path("青云山山脚", "黑风寨庭院").empty());
    EXPECT_TRUE(map.find_path("不存在的房间", "青云山山脚").empty());
}

class RoomMapFixtureTest : public ::testing::Test {
protected:
    std::filesystem::path path_;

    void SetUp() override { path_ = test_tmp_dir() / "room_map_fixture.dat"; }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    void write_fixture()
    {
        std::ofstream file(path_);
        file << "[setting]\r\nNum=4\r\nShareNum=1\r\n\r\n";
        file << "[Share_1]\r\nObj=tie\r\nCmd=/trans 0\r\nOut=乙\r\n\r\n";
        file << "[1]\r\nName1=甲\r\nName2=Jia\r\nNum=2\r\n";
        file << "Out2=丁\r\nOut1=乙\r\n\r\n";
        file << "[2]\r\nName1=乙\r\nName2=Yi\r\nNum=1\r\nOut1=丙\r\n\r\n";
        file << "[3]\r\nName1=丙\r\nName2=Bing\r\nNum=0\r\n\r\n";
        file << "[4]\r\nName1=丁\r\nName2=Ding\r\nNum=1\r\nOut1=丙\r\n";
    }
};

TEST_F(RoomMapFixtureTest, IgnoresNonRoomSectionsAndSortsExits)
{
    write_fixture();

    RoomMap map;
    ASSERT_TRUE(map.load(path_.string()));
    EXPECT_EQ(map.size(), 4u);

    // 甲有两条到丙的路，Out1=乙 即便在文件里写在 Out2=丁 之后也应优先
    auto steps = map.find_path("甲", "丙");
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].go_key, "乙");
    EXPECT_EQ(steps[1].go_key, "丙");
    EXPECT_EQ(steps[0].arrival_name, "乙");

    // Share 段里的 Out=乙 不是房间出口，不能被当成额外边
    EXPECT_EQ(map.find_path("甲", "乙").size(), 1u);

    // fixture 里的 Name2（Jia/Yi/…）不参与解析
    EXPECT_EQ(map.find_room("Jia"), -1);
}

// 显示名（Name2 / set_name 短名）全服允许重名，定位一律只认注册名（Name1）。
// rooms.dat 里 46 处显示名撞车，且某房 Name2 可能恰等于另一房的 Name1——
// 日月神教的「黑木崖小舍内室」正是被「黑木崖内室」的过期 Name2 抢走过，
// 整条路线在第 25 跳等满超时中止。
class RoomMapCollisionTest : public ::testing::Test {
protected:
    std::filesystem::path path_;

    void SetUp() override { path_ = test_tmp_dir() / "room_map_collision.dat"; }
    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    // 房 1 的 Name2 故意写成房 2 的注册名，且排在房 2 之前
    void write_collision_fixture()
    {
        std::ofstream file(path_);
        file << "[setting]\r\nNum=3\r\n\r\n";
        file << "[1]\r\nName1=黑木崖内室\r\nName2=黑木崖小舍内室\r\n";
        file << "Num=1\r\nOut1=黑木崖小舍\r\n\r\n";
        file << "[2]\r\nName1=黑木崖小舍内室\r\nName2=小舍内室\r\n";
        file << "Num=1\r\nOut1=黑木崖小舍\r\n\r\n";
        file << "[3]\r\nName1=黑木崖小舍\r\nName2=小舍\r\nNum=0\r\n";
    }
};

TEST_F(RoomMapCollisionTest, OnlyRegistrationNameResolves)
{
    write_collision_fixture();

    RoomMap map;
    ASSERT_TRUE(map.load(path_.string()));

    // 注册名照常命中
    EXPECT_EQ(map.find_room("黑木崖内室"), 0);
    EXPECT_EQ(map.find_room("黑木崖小舍内室"), 1);
    EXPECT_EQ(map.find_room("黑木崖小舍"), 2);
    EXPECT_EQ(map.find_room("不存在"), -1);

    // 显示名一律不解析，无论它是否与他房注册名撞车
    EXPECT_EQ(map.find_room("小舍内室"), -1);
    EXPECT_EQ(map.find_room("小舍"), -1);
}

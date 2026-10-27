// test_rename_notify.cpp — 改名推送：CMainObject::set_name 变化时触发
// CContainer::on_name_changed，把「物件进入场景」的 11+17 重发给同一容器内
// 的在线玩家。
//
// 背景：协议没有「改名」消息，名字只在物件进场景/玩家进场景时下发。脚本先
// Add_Box 再 set_name 时，已在场玩家拿到的仍是旧名字，combat/treebox.sce 的
// `wait object 一个奇怪的树` 就只能碰运气（玩家进山顶前箱子已刷出才通过）。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include "Container.h"
#include "Item.h"
#include "Room.h"  // CBox
#include "test_lua_env.h"
#include "test_state_recorder.h"

// 观察者用户：物品从持有者身上移出会触发 CChar::save 落盘，单测环境
// 不需要该副作用，屏蔽之（save 是虚函数）
class RecordingTestUser : public StateRecordingUser {
public:
    int save() override { return 1; }
};

class RenameNotifyTest : public ::testing::Test {
protected:
    CContainer* room          = nullptr;
    RecordingTestUser* viewer = nullptr;
    CBox* box                 = nullptr;

    void SetUp() override
    {
        ensure_lua_env();
        room   = new CContainer();
        viewer = new RecordingTestUser();
        viewer->set_name("viewer", "rv1");
        viewer->move(room);
        viewer->clear();  // 隔离入房时的 11+17 下发
    }

    void TearDown() override
    {
        // 先出容器再销毁：~CMainObject 不反链，链表里的悬垂指针会被
        // 下一次 on_leave 遍历解引用（UAF）
        if (box) {
            box->move(nullptr);
            delete box;
        }
        viewer->move(nullptr);
        delete viewer;
        delete room;
    }

    // 造一个进房间的箱子（入房即把旧名下发给在场玩家），并隔离这段下发
    CBox* MakeBoxInRoom(const char* box_id)
    {
        box = new CBox();
        box->set_name("箱子", box_id);
        box->move(room);
        viewer->clear();
        return box;
    }
};

// 场景内箱子改名：在场玩家立刻收到携带新名字的详情包（E2E 随机树箱）
TEST_F(RenameNotifyTest, BoxRenameReachesCoLocatedUser)
{
    CBox* tree = MakeBoxInRoom("rbox");

    tree->set_name("一个奇怪的树", "tree");

    EXPECT_TRUE(viewer->has("&C=11&"));          // 重发可见性
    EXPECT_TRUE(viewer->has("&C=17&"));          // 重发详情
    EXPECT_TRUE(viewer->has("N=一个奇怪的树"));  // 新名字送达
    EXPECT_TRUE(viewer->has("I=tree"));
}

// 未入场景（无环境）的对象改名：不产生任何投递
TEST_F(RenameNotifyTest, RenameWithoutEnvironmentStaysSilent)
{
    CBox box;
    box.set_name("箱子", "rbox");
    box.set_name("一个奇怪的树", "tree");

    EXPECT_EQ(0, viewer->sent);
}

// 只发给同容器内的玩家：其他房间的玩家收不到
TEST_F(RenameNotifyTest, RenameSkipsUsersOutsideTheContainer)
{
    CContainer other_room;
    StateRecordingUser stranger;
    stranger.set_name("stranger", "rs1");
    stranger.move(&other_room);
    stranger.clear();

    CBox* tree = MakeBoxInRoom("rbox");

    tree->set_name("一个奇怪的树", "tree");

    EXPECT_EQ(0, stranger.sent);
    EXPECT_GT(viewer->sent, 0);

    stranger.move(nullptr);
}

// 物品在玩家背包里改名：持有者收到
TEST_F(RenameNotifyTest, InventoryRenameReachesHolder)
{
    CItem item;
    item.set_name("旧名", "ritem");
    item.move(viewer);
    viewer->clear();  // 隔离入包时的 11+17 下发

    item.set_name("新名", "ritem");

    EXPECT_TRUE(viewer->has("N=新名"));

    item.move(nullptr);  // 出包，避免析构后持有者链表悬垂
}

// 名字与 id 都没变：不重复投递
TEST_F(RenameNotifyTest, SameNameRenameDoesNotResend)
{
    CBox* tree = MakeBoxInRoom("rbox");
    tree->set_name("一个奇怪的树", "tree");
    viewer->clear();

    tree->set_name("一个奇怪的树", "tree");

    EXPECT_EQ(0, viewer->sent);
}

// 只改 id：客户端按 id 定目标，同样要重发
TEST_F(RenameNotifyTest, IdOnlyChangeStillResends)
{
    CBox* tree = MakeBoxInRoom("rbox");
    tree->set_name("一个奇怪的树", "tree");
    viewer->clear();

    tree->set_name(nullptr, "tree2");

    EXPECT_TRUE(viewer->has("I=tree2"));
}

// test_box_door.cpp — CBox（room/Box.cpp）与 CDoor（room/Door.cpp）的行为契约
//
// 覆盖：
//   1. t_box 存档行的字段键与「行存在则 update、否则 insert」分流；
//   2. load_object 按同一套键还原箱内物件；
//   3. 上锁/开锁的耐久扣减与状态迁移；
//   4. 往箱子里放物件；
//   5. 出口绑定与按 id/方向查找；
//   6. enter_door 进房后 init_door 临时标记必须清除。
#include "test_bind_common.h"

#include "Room.h"
#include "mock/mock_database.h"

extern IDatabase* g_db;

class BoxDoorTest : public ::testing::Test {
protected:
    static constexpr const char* kTargetRoom = "boxdoor_target_room";

    CRoom env;
    MockDatabase* db = nullptr;
    CMockItemMgr mgr;
    std::unique_ptr<ItemFactory> factory;

    void SetUp() override
    {
        db      = new MockDatabase();
        g_db    = db;
        factory = std::make_unique<ItemFactory>(&mgr);
    }

    void TearDown() override
    {
        factory.reset();  // 回收工厂创建的物件
        g_db = nullptr;
        delete db;
        db = nullptr;
        g_room_list.erase(kTargetRoom);
    }

    // 让箱子处于「可存盘」房间环境中（save_object 的前置条件）
    void PlaceInSaveableRoom(CBox& box, const char* obj_id)
    {
        env.set("saveobjects", 1);
        env.set("base_name", "boxdoor_room");
        box.set("db_room_id", obj_id);
        box.move(&env);
    }
};

// ====================================================================
// CBox::save_object —— t_box 存档行格式
// ====================================================================

TEST_F(BoxDoorTest, SaveObjectWritesBoxRowFields)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_1");

    // 逐件入箱：数量>1 的非叠加物件会被 move 的防复制逻辑拒收
    CItem* sword = static_cast<CItem*>(ItemFactory::Create("sword"));
    sword->set("base_name", "sword");
    sword->move(&box);

    CItem* herb = static_cast<CItem*>(ItemFactory::Create("herb"));
    herb->set("base_name", "herb");
    herb->move(&box);

    ASSERT_TRUE(box.save_object());

    const std::string sql = db->GetLastSql();
    EXPECT_NE(sql.find("insert t_box"), std::string::npos);
    EXPECT_NE(sql.find("boxdoor_1"), std::string::npos);
    EXPECT_NE(sql.find("\"count\":2"), std::string::npos);
    EXPECT_NE(sql.find("\"0\":"), std::string::npos);  // 条目按下标嵌套
    EXPECT_NE(sql.find("\"1\":"), std::string::npos);
    EXPECT_NE(sql.find("\"name\":\"sword\""), std::string::npos);
    EXPECT_NE(sql.find("\"name\":\"herb\""), std::string::npos);
    EXPECT_NE(sql.find("\"index\""), std::string::npos);
    EXPECT_NE(sql.find("\"amount\":1"), std::string::npos);
    EXPECT_NE(sql.find("\"unique\""), std::string::npos);
}

TEST_F(BoxDoorTest, SaveObjectUpdatesWhenRowExists)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_2");

    DbResultSet existing = MockDatabase::MakeSingleRow({"existing_row"});
    db->SetQueryResultMatching("select obj_data from t_box", existing);

    ASSERT_TRUE(box.save_object());

    const std::string sql = db->GetLastSql();
    EXPECT_NE(sql.find("update t_box"), std::string::npos);
    EXPECT_NE(sql.find("where obj_id='boxdoor_2'"), std::string::npos);
}

// ====================================================================
// CBox::load_object —— 与 save_object 同一套字段键
// ====================================================================

TEST_F(BoxDoorTest, LoadObjectRestoresBoxContents)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_3");

    CMapping row;
    row.set("count", 1);
    row.set("0/name", "sword");
    row.set("0/index", 7);
    row.set("0/amount", 1);
    row.set("0/unique", "");
    std::string buffer = row.get_buffer();
    DbResultSet loaded = MockDatabase::MakeSingleRow({buffer});
    db->SetQueryResultMatching("select obj_data from t_box", loaded);

    ASSERT_EQ(1, box.load_object());

    DTItemList* list = box.get_item_list();
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(1, (int)list->size());
    EXPECT_EQ(1, list->front()->query_amount());
    EXPECT_EQ(1, list->front()->query_attr(CMainObject::IS_ITEM));
}

// ====================================================================
// CBox::add_object —— 物件入箱
// ====================================================================

TEST_F(BoxDoorTest, AddObjectPutsItemsInBox)
{
    CBox box;
    box.add_object("sword", 2);

    DTItemList* list = box.get_item_list();
    ASSERT_NE(nullptr, list);
    ASSERT_EQ(2, (int)list->size());
    for (auto* obj : *list) {
        EXPECT_EQ(1, obj->query("no_drop_man"));  // 箱内不可装人
    }
}

// ====================================================================
// CBox 上锁/开锁 —— 耐久扣减与状态迁移
// ====================================================================

TEST_F(BoxDoorTest, LockedAndUnLockedDurability)
{
    CBox box;
    char reason[] = "守卫离开前不得开启";

    box.locked(reason, 30);
    EXPECT_STREQ("locked", box.querystr("status"));
    EXPECT_EQ(30, box.query("duration"));
    EXPECT_EQ('\0', box.querystr("owner")[0]);  // 无主人的锁

    EXPECT_EQ(20, box.un_locked(10));  // 扣 10，剩 20
    EXPECT_STREQ("locked", box.querystr("status"));
    EXPECT_EQ(20, box.query("duration"));

    EXPECT_EQ(0, box.un_locked(20));  // 扣尽 → 归零并开锁
    EXPECT_STREQ("open", box.querystr("status"));
    EXPECT_EQ(0, box.query("duration"));
    EXPECT_EQ('\0', box.querystr("owner")[0]);
}

TEST_F(BoxDoorTest, UnLockedWithoutResumeOpensImmediately)
{
    CBox box;
    char reason[] = "守卫离开前不得开启";

    box.locked(reason, 50);
    EXPECT_EQ(0, box.un_locked());
    EXPECT_STREQ("open", box.querystr("status"));
    EXPECT_EQ(0, box.query("duration"));
}

// ====================================================================
// CDoor —— 绑定与出口查找
// ====================================================================

TEST_F(BoxDoorTest, BindAndPresentDoor)
{
    CRoom room;
    CDoor* door = room.add_door("east", "room/b", "west", "E");
    ASSERT_NE(nullptr, door);
    EXPECT_STREQ("open", door->querystr("status"));
    EXPECT_EQ(door, room.present_door("east"));  // 按 id
    EXPECT_EQ(door, room.present_door("E"));     // 按方向
    EXPECT_EQ(nullptr, room.present_door("north"));

    door->bind("room/c", "south");
    EXPECT_STREQ("room/c", door->querystr("next_room"));
    EXPECT_STREQ("south", door->querystr("next_door"));
    EXPECT_EQ(door, room.present_door("east"));  // 绑定不改 id
}

// ====================================================================
// CDoor::enter_door —— 进房并清除 init_door 临时标记
// ====================================================================

TEST_F(BoxDoorTest, EnterDoorMovesCharAndClearsTempFlag)
{
    CRoom target;
    target.set_name("目标房间", kTargetRoom);
    g_room_list[kTargetRoom] = &target;

    CDoor door;
    door.bind(kTargetRoom, "south");

    CChar me;
    EXPECT_EQ(1, door.enter_door(&me));
    EXPECT_EQ(&target, me.environment());
    EXPECT_EQ('\0', me.querystr_temp("init_door")[0]);
}

// ====================================================================
// CDoor::enter_door 失败支：目标房 move 自身必败 → init_door 临时标当场清除
// ====================================================================
TEST_F(BoxDoorTest, EnterDoorMoveFails_ClearsTempFlag)
{
    CRoom target;
    target.set_name("重门目标房", kTargetRoom);
    g_room_list[kTargetRoom] = &target;

    CDoor door;
    door.bind(kTargetRoom, "south");

    CChar me;
    me.set_name("重甲", "heavy1");
    target.del("infinity");  // CRoom 自带无限负重 → 摘除才进超重闸
    target.set_max_encumbrance(0);
    me.set_weight(1000);

    EXPECT_EQ(0, door.enter_door(&me)) << "超重 move 失败 → 0(19-21)";
    EXPECT_EQ('\0', me.querystr_temp("init_door")[0]) << "临时标已清(20)";
}

#include "test_log_hits.h"  // count_log_hits（DB 失败日志断言）
#include "User.h"           // save_local 向环境内用户重放

// ====================================================================
// handle_action：owner 把关 + 钥匙密码开锁链
// ====================================================================

TEST_F(BoxDoorTest, HandleActionOwnerMismatchRefusesOpen)
{
    CBox box;
    box.move(&env);
    box.locked((char*)"squatter", 60);
    box.set("owner", "someone_else");

    CChar me;
    me.set_name("开锁匠", "smith");
    me.move(&env);

    EXPECT_EQ(0, box.handle_action((char*)"open", &me, (char*)""))
        << "非主人开箱被拒(74-76)";
    EXPECT_STREQ("locked", box.querystr("status")) << "拒绝后仍锁定";
}

TEST_F(BoxDoorTest, HandleActionKeyWithMatchingPasswordUnlocks)
{
    CBox box;
    box.set_key(4242);
    box.move(&env);
    box.locked((char*)"lost", 60);

    CChar me;
    me.set_name("主人", "hero");
    me.move(&env);

    CItem* key = new CItem();
    key->set("base_name", "key");
    key->set("password", 4242);
    key->move(&me);

    EXPECT_EQ(1, box.handle_action((char*)"open", &me, (char*)""))
        << "密码吻合开锁(80-92)";
    EXPECT_STREQ("open", box.querystr("status")) << "un_locked 置 open";
    EXPECT_EQ(0, box.querystr("owner")[0]) << "owner 已清";
}

TEST_F(BoxDoorTest, HandleActionKeyWithWrongPasswordKeepsLock)
{
    CBox box;
    box.set_key(4242);
    box.move(&env);
    box.locked((char*)"lost", 60);

    CChar me;
    me.set_name("闯入者", "intruder");
    me.move(&env);

    CItem* key = new CItem();
    key->set("base_name", "key");
    key->set("password", 9999);
    key->move(&me);

    EXPECT_EQ(0, box.handle_action((char*)"open", &me, (char*)""))
        << "密码不符不放行(走函数尾 101)";
    EXPECT_STREQ("locked", box.querystr("status")) << "保持锁定";
}

// ====================================================================
// save_local / on_leave：saveobjects 落库 + 向环境内用户重放
// ====================================================================

TEST_F(BoxDoorTest, SaveLocalPersistsAndReplaysToUsers)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_sl");

    CItem* loot = new CItem();
    loot->set("base_name", "gem");
    loot->move(&box);

    CUser user;
    user.set_name("观众", "watcher");
    user.move(&env);

    EXPECT_EQ(1, box.save_local()) << "保存本地返回 1(160)";
    EXPECT_NE(db->GetLastSql().find("boxdoor_sl"), std::string::npos)
        << "saveobjects=1 → 落库(152-153)；向环境用户重放(156-158)";
}

TEST_F(BoxDoorTest, ItemLeavingBoxSavesWhenSaveobjectsEnabled)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_leave");

    CItem* loot = new CItem();
    loot->set("base_name", "relic");
    loot->move(&box);
    loot->move(nullptr);  // 件离箱 → CBox::on_leave 通知并落库
    delete loot;

    EXPECT_NE(db->GetLastSql().find("boxdoor_leave"), std::string::npos)
        << "箱内物品离箱触发落库(144)";
}

// ====================================================================
// DB 失败面：坏缓冲 / 保存失败 / 删除失败 各记一条错误日志
// ====================================================================

TEST_F(BoxDoorTest, LoadObjectToleratesCorruptBuffer)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_buf");

    DbResultSet rs = MockDatabase::MakeSingleRow({"@@not-a-buffer@@"});
    db->SetQueryResultMatching("select obj_data from t_box", std::move(rs));

    const int errs = count_log_hits("CBox::LoadObject");
    EXPECT_TRUE(box.load_object())
        << "坏缓冲按空箱宽容成功（!(-1) 吞掉失败返回）";
    EXPECT_EQ(count_log_hits("CBox::LoadObject"), errs)
        << "宽容路径不记错误日志";
}

TEST_F(BoxDoorTest, DbFailuresLogSaveAndRemoveErrors)
{
    CBox box;
    PlaceInSaveableRoom(box, "boxdoor_dbfail");
    db->SetShouldFail(true);

    const int s_before = count_log_hits("CBox::SaveObject: db_save_container");
    box.save_object();
    EXPECT_EQ(count_log_hits("CBox::SaveObject: db_save_container"),
              s_before + 1)
        << "保存失败记日志(241)";

    const int r_before = count_log_hits("CBox::RemoveObjects: db_delete");
    box.remove_objects();
    EXPECT_EQ(count_log_hits("CBox::RemoveObjects: db_delete"), r_before + 1)
        << "删除失败记日志(251)";
}

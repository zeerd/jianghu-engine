// test_bind_container_misc.cpp — binds in ContainerWeight.cpp +

// ContainerMisc.cpp

#include "test_bind_common.h"

#include "User.h"

#include "Equip.h"  // CEquip 实现 IEquippable/IDialogSource

#include "test_state_recorder.h"  // 投递观察

class BindContainerMiscTest : public BindTest {
protected:
    CContainer c;
};

// ---- ContainerWeight.cpp ----

TEST_F(BindContainerMiscTest, SetWeight)

{
    lua_bind_push_object(L, &c);

    lua_pushinteger(L, 250);

    CallVoid(l_set_weight, 2);

    EXPECT_EQ(250, c.weight());
}

TEST_F(BindContainerMiscTest, QueryAmountWeight)

{
    lua_bind_push_object(L, &c);

    EXPECT_GE(CallInt(l_query_amount, 1), 0);

    lua_bind_push_object(L, &c);

    EXPECT_GE(CallInt(l_query_weight, 1), 0);
}

TEST_F(BindContainerMiscTest, IsCharacterFalse)

{
    lua_bind_push_object(L, &c);

    EXPECT_FALSE(CallBool(l_is_character, 1));
}

TEST_F(BindContainerMiscTest, CanEquipped)

{
    // 普通容器不具备 CAN_EQUIPPED

    lua_bind_push_object(L, &c);

    EXPECT_FALSE(CallBool(l_can_equipped, 1));
}

TEST_F(BindContainerMiscTest, update_me)

{
    // 契约：update_me 自增 UPDATE 计数（ContainerWeight.cpp m_update++）

    int32_t before = c.query_attr(CChar::UPDATE);

    lua_bind_push_object(L, &c);

    EXPECT_EQ(0, CallRetCount(l_update_me, 1));

    EXPECT_EQ(before + 1, c.query_attr(CChar::UPDATE));
}

// ---- ContainerMisc.cpp ----

TEST_F(BindContainerMiscTest, SaveLoadLockData)

{
    lua_bind_push_object(L, &c);

    int64_t r = CallInt(l_save, 1);

    EXPECT_GE(r, 0);

    lua_bind_push_object(L, &c);

    CallInt(l_load, 1);

    lua_bind_push_object(L, &c);

    lua_pushinteger(L, 1);

    CallInt(l_lockdata, 2);
}

TEST_F(BindContainerMiscTest, WieldWear)

{
    lua_bind_push_object(L, &c);

    CallInt(l_wield, 1);

    lua_bind_push_object(L, &c);

    CallInt(l_wear, 1);
}

TEST_F(BindContainerMiscTest, MenuAndDownload)

{
    CUser u;

    lua_bind_push_object(L, &c);

    lua_bind_push_object(L, &u);

    CallVoid(l_send_menu, 2);

    lua_bind_push_object(L, &c);

    lua_pushstring(L, "look");

    lua_pushstring(L, "look");

    lua_bind_push_object(L, &u);

    CallVoid(l_add_menu_item, 4);

    lua_bind_push_object(L, &c);

    lua_bind_push_object(L, &u);

    CallVoid(l_download, 2);
}

// ---- 成功路径：CEquip 命中 IEquippable/IDialogSource/IClientObject 接口分发

// ----

TEST_F(BindContainerMiscTest, WieldWearOnEquip)

{
    CEquip eq;

    eq.set_name("佩剑", "cm_sword");

    lua_bind_push_object(L, &eq);

    int k = call(l_wield, 1);

    ASSERT_EQ(1, k) << "wield 命中接口并返回数值(341-342)";

    lua_pop(L, k);

    lua_bind_push_object(L, &eq);

    k = call(l_wear, 1);

    ASSERT_EQ(1, k) << "wear 命中接口并返回数值(352-353)";

    lua_pop(L, k);
}

TEST_F(BindContainerMiscTest, MenuAndDownloadOnEquip)

{
    CEquip eq;

    eq.set_name("宝匣", "cm_chest");

    StateRecordingUser u;

    u.set_name("食客", "cm_u");

    u.move(&c);

    u.clear();

    // send_menu(me)(367-368)

    lua_bind_push_object(L, &eq);

    lua_bind_push_object(L, &u);

    CallVoid(l_send_menu, 2);

    EXPECT_GT(u.sent, 0) << "send_menu 投递到 me(367-368)";

    // add_menu_item(show, cmd, me)(381-386)

    u.clear();

    lua_bind_push_object(L, &eq);

    lua_pushstring(L, "查看");

    lua_pushstring(L, "look");

    lua_bind_push_object(L, &u);

    CallVoid(l_add_menu_item, 4);

    EXPECT_TRUE(u.has("&c=look&")) << "菜单项组包投递(382-386)";

    // download(me)(330-331)

    lua_bind_push_object(L, &eq);

    lua_bind_push_object(L, &u);

    CallVoid(l_download, 2);

    u.move(nullptr);
}

// ---- ContainerItem.cpp: l_say 参数链与定向投递(252-257) ----
TEST_F(BindContainerMiscTest, SayBindDeliversToViewer)
{
    CEquip eq;
    eq.set_name("低语匣", "say_box");
    eq.move(&c);

    StateRecordingUser viewer;
    viewer.set_name("听者", "say_viewer");
    viewer.move(&c);
    viewer.clear();

    lua_bind_push_object(L, &eq);
    lua_pushstring(L, "低语回荡在耳边");
    lua_bind_push_object(L, &viewer);
    CallVoid(l_say, 3);

    EXPECT_TRUE(viewer.has("低语回荡在耳边")) << "l_say → 定向投递(256)";
    viewer.move(nullptr);
}

// ---- l_say 第 4 参 tips：覆盖抬头名（如「房屋帐单」），缺省回落发言者名 ----
TEST_F(BindContainerMiscTest, SayBindTipsOverridesTitle)
{
    CEquip eq;
    eq.set_name("帐单匣", "say_tips_room");
    eq.move(&c);

    StateRecordingUser viewer;
    viewer.set_name("听者", "say_tips_viewer");
    viewer.move(&c);
    viewer.clear();

    // 无 tips：抬头用发言者名
    lua_bind_push_object(L, &eq);
    lua_pushstring(L, "结算单正文");
    lua_bind_push_object(L, &viewer);
    CallVoid(l_say, 3);
    EXPECT_TRUE(viewer.has("&n=帐单匣&")) << "缺省抬头取发言者名";

    viewer.clear();
    lua_bind_push_object(L, &eq);
    lua_pushstring(L, "结算单正文");
    lua_bind_push_object(L, &viewer);
    lua_pushstring(L, "房屋帐单");
    CallVoid(l_say, 4);
    EXPECT_TRUE(viewer.has("&n=房屋帐单&")) << "tips 覆盖抬头";

    viewer.move(nullptr);
    eq.move(nullptr);
}

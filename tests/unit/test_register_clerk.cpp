// test_register_clerk.cpp — lua-default/npc/register_clerk.lua 角色注册专员
//
// 契约（交互模式参考"凶猛的狗铸造兵刃"）：
//   - do_talk 列出注册话题菜单；菜单回灌命令进入 handle_action
//   - 分步收集：regchar 开始 → regname/regpass/reggender/regstat → regdone
//   - 未开始注册（无 regclerk/step）时，注册命令一律回退基类（返回 0）
//   - regdone 调 register_char 落库，成功后清临时状态并提示账号
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>

#include "Channel.h"
#include "Char.h"
#include "Container.h"
#include "Global.h"
#include "IDatabase.h"
#include "Npc.h"
#include "NpcMgr.h"
#include "Room.h"
#include "Sql.h"
#include "User.h"
#include "lua/LuaNpc.h"
#include "mock/mock_database.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern IDatabase* g_db;
extern MAP2ROOM g_room_list;

namespace {

class RegisterClerkTest : public ::testing::Test {
protected:
    MockDatabase* db = nullptr;
    CMockItemMgr item_mgr;
    IItemMgr* saved_mgr    = nullptr;
    CRoom* room            = nullptr;
    CNpc* clerk            = nullptr;
    StateRecordingUser* me = nullptr;

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);

        saved_mgr  = g_item_mgr;
        g_item_mgr = &item_mgr;
        db         = new MockDatabase();
        g_db       = db;
        // max(cc_id)=500 → 注册成功后账号 501
        db->SetQueryResultMatching("select max(cc_id) from jianghudb",
                                   MockDatabase::MakeSingleRow({"500"}));
        // 新代码使用 LAST_INSERT_ID()，预设返回 501
        db->SetQueryResultMatching("LAST_INSERT_ID()",
                                   MockDatabase::MakeSingleRow({"501"}));

        room = new CRoom();
        room->set_name("专员测试房", "register_clerk_test_room");
        room->set("infinity", 1);

        clerk = load_npc("register_clerk");
        ASSERT_NE(clerk, nullptr);
        clerk->move(room);

        me = new StateRecordingUser();
        me->set_name("过客", "regclerk_guest");
        me->set("guest", 1);
        me->move(room);
        me->delete_temp("client/loading");
        me->clear();
    }

    void TearDown() override
    {
        me->move(nullptr);
        delete me;

        g_npc_list.erase(clerk->object_id());
        clerk->set_heart_beat(0);
        clerk->move(nullptr);
        delete clerk;

        delete room;

        g_db = nullptr;
        delete db;
        db         = nullptr;
        g_item_mgr = saved_mgr;
    }

    // 驱动专员的三参 handle_action（菜单回灌命令的入口）
    int Act(const char* comm, const char* arg = "")
    {
        char c[64], a[256];
        snprintf(c, sizeof(c), "%s", comm);
        snprintf(a, sizeof(a), "%s", arg);
        return clerk->handle_action(c, me, a);
    }

    // 一口气把注册流程推到指定步骤之后
    void DoFullFlow()
    {
        Act("regchar");
        Act("regname", "测试小侠");
        Act("regpass", "secret88");
        Act("regemail", "test@example.com");
        Act("reggender", "男性");
        Act("regstat", "21 21 21 21");
    }
};

// ---------------- 身份与 talk 菜单 ----------------
TEST_F(RegisterClerkTest, Create_SetsClerkIdentity)
{
    EXPECT_STREQ("注册专员", clerk->name());
    EXPECT_STREQ("游戏角色注册专员", clerk->querystr("title"));
}

TEST_F(RegisterClerkTest, DoTalk_ShowsRegistrationTopic)
{
    char ask[32] = "创建角色";
    EXPECT_EQ(1, clerk->do_talk(me, ask));
    EXPECT_EQ(1, me->query_temp("regclerk/talked"));
    EXPECT_TRUE(me->has("&s=与注册专员谈关于创建角色的事"))
        << "talk 菜单包含创建角色话题";
    EXPECT_TRUE(me->has("regchar")) << "菜单项回灌 regchar 命令";
}

// ---------------- 分步收集 ----------------
TEST_F(RegisterClerkTest, Regchar_StartsFlowAndShowsMenu)
{
    EXPECT_EQ(1, Act("regchar"));
    EXPECT_EQ(1, me->query_temp("regclerk/step"));
    EXPECT_TRUE(me->has("regname")) << "注册菜单包含设置姓名";
    EXPECT_TRUE(me->has("regpass")) << "注册菜单包含设置密码";
    EXPECT_TRUE(me->has("reggender")) << "注册菜单包含选择性别";
    EXPECT_TRUE(me->has("regstat")) << "注册菜单包含设置天赋";
    EXPECT_TRUE(me->has("regdone")) << "注册菜单包含确认创建";

    // 格式规则必须在首次输入之前就可见，不能等玩家答错才提示
    EXPECT_TRUE(me->has("&s=设置登录密码（4~22 个字符）"))
        << "菜单项直接给出密码长度规则";
    EXPECT_TRUE(me->has("&s=设置邮箱地址（须含 @"))
        << "菜单项直接给出邮箱格式规则";
    EXPECT_TRUE(me->has("&s=设置天赋属性（依次为力量、智慧、体质、敏捷"))
        << "菜单项直接给出四维属性顺序";
    EXPECT_TRUE(me->has("单项 10~30、总和 84"))
        << "菜单项直接给出单项区间与总和";
}

TEST_F(RegisterClerkTest, Regname_StoresValidName)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regname", "测试小侠"));
    EXPECT_STREQ("测试小侠", me->querystr_temp("regclerk/name"));
    EXPECT_TRUE(me->has("Ok."));
}

TEST_F(RegisterClerkTest, Regname_IllegalNameRejectedAndNotStored)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regname", "x"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/name"))
        << "非法名字不得入暂存";
    EXPECT_TRUE(me->has("对不起")) << "非法名字给出提示";
}

TEST_F(RegisterClerkTest, Regpass_StoresValidPassword)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regpass", "secret88"));
    EXPECT_STREQ("secret88", me->querystr_temp("regclerk/pass"));
}

TEST_F(RegisterClerkTest, Regpass_TooShortRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regpass", "abc"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/pass"));
    EXPECT_TRUE(me->has("4")) << "密码长度提示含下限";
}

TEST_F(RegisterClerkTest, Reggender_MissingArgShowsGenderMenu)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("reggender", ""));
    EXPECT_STREQ("", me->querystr_temp("regclerk/gender"));
    EXPECT_TRUE(me->has("&s=男性")) << "列出男性选项";
    EXPECT_TRUE(me->has("&s=女性")) << "列出女性选项";
}

TEST_F(RegisterClerkTest, Reggender_StoresChoice)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("reggender", "男性"));
    EXPECT_STREQ("男性", me->querystr_temp("regclerk/gender"));
}

TEST_F(RegisterClerkTest, Regstat_StoresPlayerSpecifiedStats)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regstat", "20 21 22 21"));
    EXPECT_EQ(20, me->query_temp("regclerk/str"));
    EXPECT_EQ(21, me->query_temp("regclerk/int"));
    EXPECT_EQ(22, me->query_temp("regclerk/con"));
    EXPECT_EQ(21, me->query_temp("regclerk/dex"));
    EXPECT_TRUE(me->has("Ok."));
}

TEST_F(RegisterClerkTest, Regstat_BadSumRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regstat", "30 30 30 30"));
    EXPECT_EQ(0, me->query_temp("regclerk/str")) << "非法天赋不得入暂存";
    EXPECT_TRUE(me->has("84")) << "提示总和须为 84";
}

TEST_F(RegisterClerkTest, Regstat_OutOfRangeRejected)
{
    Act("regchar");
    me->clear();
    // 9 与 33 都越界（虽然总和正好 84）
    EXPECT_EQ(1, Act("regstat", "9 21 21 33"));
    EXPECT_EQ(0, me->query_temp("regclerk/str"));
    EXPECT_TRUE(me->has("10")) << "提示单项区间 10~30";
}

TEST_F(RegisterClerkTest, Regemail_StoresValidEmail)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regemail", "test@example.com"));
    EXPECT_STREQ("test@example.com", me->querystr_temp("regclerk/email"));
    EXPECT_TRUE(me->has("Ok."));
}

TEST_F(RegisterClerkTest, Regemail_InvalidFormatRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regemail", "invalid-email"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/email"))
        << "非法邮箱不得入暂存";
    EXPECT_TRUE(me->has("格式")) << "非法邮箱给出提示";
    EXPECT_TRUE(me->has("须包含 @，且 @ 前后不能为空"))
        << "错误提示解释邮箱规则，玩家不必靠猜";
}

TEST_F(RegisterClerkTest, Regemail_MissingAtStartRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regemail", "@example.com"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/email"));
    EXPECT_TRUE(me->has("格式"));
}

TEST_F(RegisterClerkTest, Regemail_MissingAtEndRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regemail", "user@"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/email"));
    EXPECT_TRUE(me->has("格式"));
}

TEST_F(RegisterClerkTest, Regstat_NotFourNumbersRejected)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regstat", "21 21 21"));
    EXPECT_EQ(0, me->query_temp("regclerk/str"));
    EXPECT_EQ(1, Act("regstat", "abc"));
    EXPECT_EQ(0, me->query_temp("regclerk/str"));
    EXPECT_TRUE(me->has("四个数字")) << "提示输入格式";
    // 清屏之后只剩错误提示，首次答错就要把全部规则一次说清
    EXPECT_TRUE(me->has("依次为力量、智慧、体质、敏捷"))
        << "错误提示给出四维属性顺序";
    EXPECT_TRUE(me->has("单项 10~30，四项总和必须正好 84"))
        << "错误提示一次给出区间与总和";
}

// ---------------- 门槛与确认 ----------------
TEST_F(RegisterClerkTest, CommandsBeforeRegchar_AreIgnored)
{
    // 未开始注册：注册命令回退基类返回 0，且不得写任何暂存
    EXPECT_EQ(0, Act("regname", "测试小侠"));
    EXPECT_EQ(0, Act("regpass", "secret88"));
    EXPECT_EQ(0, Act("regemail", "test@example.com"));
    EXPECT_EQ(0, Act("reggender", "男性"));
    EXPECT_EQ(0, Act("regstat", "21 21 21 21"));
    EXPECT_EQ(0, Act("regdone"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/name"));
    EXPECT_EQ(0, me->query("regclerk/done"));
}

TEST_F(RegisterClerkTest, Regdone_MissingFields_RefusesAndKeepsFlow)
{
    Act("regchar");
    me->clear();
    EXPECT_EQ(1, Act("regdone"));
    EXPECT_EQ(0, me->query("regclerk/done")) << "缺信息不得完成注册";
    EXPECT_EQ(1, me->query_temp("regclerk/step")) << "注册流程继续";
    EXPECT_FALSE(db->ExecutedContaining("insert into jianghudb"));
    EXPECT_TRUE(me->has("姓名")) << "提示缺哪个字段";
    EXPECT_TRUE(me->has("邮箱")) << "提示缺邮箱字段";
}

TEST_F(RegisterClerkTest, Regdone_FullFlow_CreatesAccountAndClearsState)
{
    DoFullFlow();
    me->clear();
    EXPECT_EQ(1, Act("regdone"));

    EXPECT_EQ(1, me->query("regclerk/done"));
    EXPECT_EQ(0, me->query_temp("regclerk/step")) << "流程状态已清";
    EXPECT_STREQ("", me->querystr_temp("regclerk/name"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/pass"));
    EXPECT_STREQ("", me->querystr_temp("regclerk/email"));

    EXPECT_TRUE(db->ExecutedContaining("insert into jianghudb"));
    EXPECT_TRUE(db->ExecutedContaining("'测试小侠'")) << "名字入库";
    EXPECT_TRUE(db->ExecutedContaining("'secret88'")) << "密码入库";
    EXPECT_TRUE(db->ExecutedContaining("'test@example.com'")) << "邮箱入库";
    EXPECT_TRUE(db->ExecutedContaining("'男性'")) << "性别入库";
    EXPECT_TRUE(db->ExecutedContaining("cc_email")) << "含邮箱字段";

    EXPECT_TRUE(me->has("注册成功")) << "播报注册成功";
    EXPECT_TRUE(me->has("501")) << "播报新账号";
}

TEST_F(RegisterClerkTest, Regdone_DbFailure_ReportsErrorAndStaysInFlow)
{
    DoFullFlow();
    db->SetShouldFail(true);
    me->clear();

    EXPECT_EQ(1, Act("regdone"));
    EXPECT_EQ(0, me->query("regclerk/done")) << "落库失败不得视为注册成功";
    EXPECT_EQ(1, me->query_temp("regclerk/step")) << "留在注册流程中";
    EXPECT_TRUE(me->has("注册失败")) << "把错误播报给玩家";
}

// ---------------- rk 房间白名单（lua-default/room/rk.lua） ----------------
TEST_F(RegisterClerkTest, RkRoom_AllowsRegistrationAndBlocksOthers)
{
    CRoom* rk = load_room("rk");
    ASSERT_NE(rk, nullptr);
    EXPECT_EQ(1, rk->query("wiz_room")) << "rk 是巫师/特殊房";

    // 找到 rk 自己的专员（fixture 的专员在另一间房，按环境区分）
    CNpc* rk_clerk = nullptr;
    for (auto& kv : g_npc_list) {
        if (EQUALSTR(kv.second->querystr("base_name"), "register_clerk")
            && kv.second->environment() == rk) {
            rk_clerk = kv.second;
            break;
        }
    }
    ASSERT_NE(nullptr, rk_clerk) << "rk 房间 create 时应载入注册专员";

    char tar[24];
    snprintf(tar, sizeof(tar), "%d", rk_clerk->object_id());

    me->move(rk);
    me->clear();

    // 白名单命令：放行（返回 0），由命令分发器的目标截获环节派发
    char password[] = "password";
    char empty[]    = "";
    EXPECT_EQ(0, rk->handle_action(password, me, empty, tar))
        << "password 放行";

    char talk[] = "talk";
    EXPECT_EQ(0, rk->handle_action(talk, me, empty, tar)) << "talk 放行";

    char regname[] = "regname";
    char name[]    = "某某";
    EXPECT_EQ(0, rk->handle_action(regname, me, name, tar))
        << "白名单内命令放行，交给目标截获环节";
    EXPECT_FALSE(me->has("没有信号"));

    // register 快捷命令：无需目标直接开始注册
    me->delete_temp("regclerk/step");
    char reg[] = "register";
    EXPECT_EQ(1, rk->handle_action(reg, me, empty, tar));
    EXPECT_EQ(1, me->query_temp("regclerk/step"))
        << "register 快捷命令进入注册流程";

    // 白名单外命令：拦截并提示
    me->clear();
    char look[] = "look";
    EXPECT_EQ(1, rk->handle_action(look, me, empty, tar));
    EXPECT_TRUE(me->has("没有信号")) << "非白名单命令被大厅拦截";

    // 点卡命令已随点卡系统移除，按普通命令拦截
    char card[] = "card";
    EXPECT_EQ(1, rk->handle_action(card, me, empty, tar))
        << "card 已不是放行命令";

    me->move(nullptr);

    // 清理 rk 房间的专员与房间本体
    g_npc_list.erase(rk_clerk->object_id());
    rk_clerk->set_heart_beat(0);
    rk_clerk->move(nullptr);
    delete rk_clerk;
    g_room_list.erase("rk");
    delete rk;
}

}  // namespace

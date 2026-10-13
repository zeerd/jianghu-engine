// test_emote_system.cpp — CEmoteSystem：表情表加载、渲染与下发
//
// 契约：
//  - load_emotes_from_lua 用 data/emote.lua 重建表情表
//  - do_emote 依次渲染 myself / target / others 三段文案：频道模式返回
//    others 文案，直接模式投递给本人与目标后返回空串
//  - 直接模式下目标必须与发出者同处一室；自目标改走 _self 文案
//  - 目标收到 relay_emote 回执；动词表整表下发给客户端
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <cstring>
#include <string>
#include <utility>

#include "Char.h"
#include "Container.h"
#include "EmoteSystem.h"
#include "User.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

namespace {

// 记录 NPC 收到的表情回执，证明 relay_emote 确实被调用
class RelaySpyChar : public CChar {
public:
    void relay_emote(CChar* me, std::string verb) override
    {
        relay_count++;
        last_verb = std::move(verb);
    }

    int relay_count = 0;
    std::string last_verb;
};

class EmoteRenderTest : public ::testing::Test {
protected:
    CEmoteSystem es;
    CContainer env;

    void SetUp() override
    {
        ensure_lua_env();
        es.load_emotes_from_lua();
    }

    static void EnterRoom(CChar* ob, const char* name, const char* id,
                          CContainer* room)
    {
        ob->set_name(name, id);
        ob->move(room);
    }
};

}  // namespace

// 频道模式：无目标时返回 others 文案，$N 已替换为发出者姓名
TEST_F(EmoteRenderTest, ChannelModeNoTarget_RendersOthersText)
{
    CChar me;
    EnterRoom(&me, "张三", "emote_me", &env);

    char* out = es.do_emote(&me, nullptr, "watermelon", 1);

    ASSERT_NE(out, nullptr) << "表情表未从 Lua 载入或渲染失败";
    EXPECT_TRUE(std::strstr(out, "今日我请大家吃西瓜") != nullptr) << out;
    EXPECT_TRUE(std::strstr(out, "张三") != nullptr) << out;

    me.move(nullptr);
}

// 直接模式：本人视角与全场文案走投递，最终返回空串
TEST_F(EmoteRenderTest, DirectModeNoTarget_TellsSenderAndReturnsEmpty)
{
    CChar me;
    EnterRoom(&me, "张三", "emote_me", &env);

    char* out = es.do_emote(&me, nullptr, "watermelon", 0);

    ASSERT_NE(out, nullptr);
    EXPECT_STREQ(out, "");

    me.move(nullptr);
}

// 直接模式：同室目标收到 target 文案与 relay_emote 回执
TEST_F(EmoteRenderTest, DirectModeWithTarget_RendersAndRelaysToTarget)
{
    CChar me;
    RelaySpyChar target;
    EnterRoom(&me, "张三", "emote_me", &env);
    EnterRoom(&target, "李四", "emote_tgt", &env);

    char* out = es.do_emote(&me, &target, "watermelon", 0);

    ASSERT_NE(out, nullptr);
    EXPECT_STREQ(out, "");
    EXPECT_EQ(target.relay_count, 1) << "目标必须收到表情回执";
    EXPECT_EQ(target.last_verb, "watermelon");

    me.move(nullptr);
    target.move(nullptr);
}

// 直接模式：异地目标在渲染前被拒绝，且不会收到回执
TEST_F(EmoteRenderTest, DirectModeTargetInAnotherRoom_Rejected)
{
    CChar me;
    RelaySpyChar other;
    CContainer elsewhere;
    EnterRoom(&me, "张三", "emote_me", &env);
    EnterRoom(&other, "王五", "emote_else", &elsewhere);

    EXPECT_EQ(es.do_emote(&me, &other, "watermelon", 0), nullptr);
    EXPECT_EQ(other.relay_count, 0);

    me.move(nullptr);
    other.move(nullptr);
}

// 自目标改用 _self 文案（others_self 段）
TEST_F(EmoteRenderTest, SelfTarget_UsesSelfPostfix)
{
    CChar me;
    EnterRoom(&me, "张三", "emote_me", &env);

    char* out = es.do_emote(&me, &me, "watermelon", 1);

    ASSERT_NE(out, nullptr);
    EXPECT_TRUE(std::strstr(out, "猴急") != nullptr) << out;

    me.move(nullptr);
}

// 玩家发出者：$N 渲染成 姓名(id)；情侣身份不改变目标名渲染
TEST_F(EmoteRenderTest, UserEmitter_RendersNameWithId)
{
    StateRecordingUser me;
    StateRecordingUser target;
    EnterRoom(&me, "张三", "emote_me", &env);
    EnterRoom(&target, "李四", "emote_tgt", &env);
    me.clear();
    target.clear();

    char* out = es.do_emote(&me, &target, "watermelon", 1);
    ASSERT_NE(out, nullptr);
    EXPECT_TRUE(std::strstr(out, "张三(emote_me)") != nullptr) << out;
    EXPECT_TRUE(std::strstr(out, "李四") != nullptr) << out;

    me.set("couple/id", "emote_tgt");
    me.set("couple/gender", "娘子");
    char* coupled = es.do_emote(&me, &target, "watermelon", 1);
    ASSERT_NE(coupled, nullptr);
    EXPECT_TRUE(std::strstr(coupled, "李四") != nullptr)
        << "情侣身份不得改写目标名渲染";

    me.move(nullptr);
    target.move(nullptr);
}

// 动词表整表下发：每条协议命令携带动词名
TEST_F(EmoteRenderTest, DownloadEmote_SendsEveryVerb)
{
    StateRecordingUser user;
    user.set_name("张三", "emote_dl");
    user.move(&env);
    user.clear();

    es.download_emote(&user);

    EXPECT_GT(user.sent, 1);
    EXPECT_TRUE(user.has("&e=watermelon&")) << "watermelon 动词未下发";
    EXPECT_TRUE(user.has("&e=face&")) << "face 动词未下发";

    user.move(nullptr);
}

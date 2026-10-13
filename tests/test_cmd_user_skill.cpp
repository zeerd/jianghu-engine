// test_cmd_user_skill.cpp - Unit tests for CmdUserSkill.cpp
//
// skill（单数）由 CCmdUser 用户命令表处理；skills 经 CCmdHandle 完整分发链
// （C++ 三表未命中后回落 Lua commands 表）验证——迁移前后同一可观察行为，
// 即功能不变的锚点。
//
#include "pch.h"

#include <string>
#include <vector>

#include "Char.h"
#include "User.h"
#include "Room.h"
#include "Skill.h"
#include "SkillMgr.h"
#include "Global.h"
#include "CmdUser.h"
#include "CmdHandle.h"
#include "mock/mock_itemmgr.h"
#include "test_lua_env.h"
#include "test_state_recorder.h"

extern char g_notify_fail_msg[512];

namespace {
// 走完整分发链（含 Lua commands 回落）的公开入口。
class SkillDispatchHarness : public CCmdHandle {
public:
    int Dispatch(const char* cmd, CChar* me, int32_t target, const char* arg)
    {
        return handlecommand(const_cast<char*>(cmd), me, target,
                             const_cast<char*>(arg));
    }
};
}  // namespace

class CmdUserSkillTest : public ::testing::Test {
protected:
    CCmdUser cmd;
    CChar* me;
    CContainer* room;

    void SetUp() override
    {
        ensure_lua_env();
        room = new CContainer();
        room->set("name", "test_room");

        me = new CChar();
        me->set("name", "测试者");
        me->set("id", "tester1");
        me->set("level", (long)50);
        me->set("hp", (long)100);
        me->move(room);
    }

    void TearDown() override
    {
        me->move(nullptr);
        room->move(nullptr);
        delete me;
        delete room;
    }
};

TEST_F(CmdUserSkillTest, Info_ViaHandlecommand)
{
    int result = cmd.handlecommand((char*)"skill", me, 0, (char*)"");
    EXPECT_NE(result, -1);
}

// ---------------- skills：经完整分发链（C++ 表未命中后回落 Lua）
// ----------------

class CmdUserSkillsTest : public ::testing::Test {
protected:
    SkillDispatchHarness harness;
    CContainer room;
    StateRecordingUser* me;
    CChar* other;
    std::vector<std::string> m_skills;

    void SetUp() override
    {
        ensure_lua_env();
        g_notify_fail_msg[0] = '\0';

        room.set_name("skills_room", "skills_room");

        me = new StateRecordingUser();
        me->set_name("测试者", "tester1");
        me->move(&room);

        other = new CChar();
        other->set_name("路人", "passerby1");
        other->move(&room);
    }

    void TearDown() override
    {
        // 摘除用例经 set_skill 落入全局技能缓存的对象，避免污染后续用例
        for (const std::string& name : m_skills) {
            auto it = g_skill_list.find(name);
            if (it == g_skill_list.end()) continue;
            delete it->second;
            g_skill_list.erase(it);
        }
        m_skills.clear();

        me->move(nullptr);
        other->move(nullptr);
        delete me;
        delete other;
        room.move(nullptr);
    }

    // 学一门技能；名字登记下来，用例结束统一清理。
    void Learn(CChar* who, const std::string& skill, int level)
    {
        who->set_skill(skill.c_str(), level);
        m_skills.push_back(skill);
    }

    // 每个用例用互不相同的技能名，避免全局技能缓存跨用例串味。
    static std::string UniqueSkill(int tag)
    {
        return "cmdsk_" + std::to_string(tag);
    }
};

// 低于玩家权限：用户命令表不参与分发，回落 Lua 后权限门同样拒绝，返回 0。
TEST_F(CmdUserSkillsTest, BelowPlayerGrantFallsThrough)
{
    Learn(me, UniqueSkill(7), 10);
    me->set("grant", (long)10);
    me->clear();

    EXPECT_EQ(harness.Dispatch("skills", me, 0, (char*)""), 0);
    EXPECT_EQ(me->sent, 0);
}

// =====================================================================
// skill / downloadskill / downloadobject / downloaduser：
// 视图权限、技能下载、物件下载、玩家信息下行
// =====================================================================
class CmdUserSkillCmdTest : public ::testing::Test {
protected:
    StateRecordingUser* me;  // 玩家视角（录制下行封包）
    CChar* other;            // 房间里的旁人
    CContainer* room;
    IItemMgr* saved_mgr = nullptr;
    CMockItemMgr* mgr   = nullptr;
    std::vector<std::string> m_loaded_skills;  // load_skill 会写全局技能表

    static int CallCmd(CChar* who, const char* name, int32_t target, char* arg)
    {
        CCmdUser c;
        return c.handlecommand(const_cast<char*>(name), who, target, arg);
    }

    void SetUp() override
    {
        ASSERT_NE(ensure_lua_env(), nullptr);
        g_notify_fail_msg[0] = '\0';
        // find_object 未命中会落到 g_item_mgr->find_item（默认空指针）
        saved_mgr  = g_item_mgr;
        mgr        = new CMockItemMgr();
        g_item_mgr = mgr;

        room = new CContainer();
        room->set_name("试炼场", "skillcmd_test_room");
        room->set("infinity", 1);

        me = new StateRecordingUser();
        me->set_name("学徒", "tester1");
        me->move(room);
        me->delete_temp("client/loading");

        other = new CChar();
        other->set_name("路人", "passerby1");
        other->move(room);
    }

    void TearDown() override
    {
        // 摘掉 load_skill 登记进全局技能表的对象
        for (const std::string& name : m_loaded_skills) {
            auto it = g_skill_list.find(name);
            if (it == g_skill_list.end()) continue;
            delete it->second;
            g_skill_list.erase(it);
        }
        m_loaded_skills.clear();
        other->move(nullptr);
        delete other;
        me->move(nullptr);
        g_user_list.erase(me->object_id());
        delete me;
        room->move(nullptr);
        delete room;
        g_item_mgr = saved_mgr;
        delete mgr;
        mgr = nullptr;
    }
};

TEST_F(CmdUserSkillCmdTest, Skill_SelfViewsOwnSkills)
{
    me->set_skill("throwing", 10);
    EXPECT_EQ(1, CallCmd(me, "skill", 0, (char*)""));
    EXPECT_GT(me->sent, 0) << "自身技能下行有封包";
}

TEST_F(CmdUserSkillCmdTest, Skill_WizardViewsOther)
{
    me->set("grant", (long)CChar::GRANT_WIZARD);
    other->set_skill("unarmed", 5);
    EXPECT_EQ(1, CallCmd(me, "skill", other->object_id(), (char*)""));
    EXPECT_GT(me->sent, 0) << "巫师看他人技能有下行";
}

TEST_F(CmdUserSkillCmdTest, Skill_NoPermission_Rejected)
{
    int before = me->sent;  // sent 计数含入房封包：用差值断言"无新下行"
    EXPECT_EQ(0, CallCmd(me, "skill", other->object_id(), (char*)""));
    EXPECT_NE(std::string::npos,
              std::string(g_notify_fail_msg).find("无权查看对方技能"));
    EXPECT_EQ(before, me->sent) << "被拒时无下行";
}

TEST_F(CmdUserSkillCmdTest, DownloadSkill_FixtureScript_Downloads)
{
    m_loaded_skills.push_back("tc_throwing");  // 登记以便收尾清理
    EXPECT_EQ(1, CallCmd(me, "downloadskill", 0, (char*)"tc_throwing"));
    EXPECT_TRUE(me->has("S=tc_throwing")) << "技能详情封包带脚本名";
}

TEST_F(CmdUserSkillCmdTest, DownloadObject_KnownUser_Downloads)
{
    auto* victim = new StateRecordingUser();
    victim->set_name("被查看者", "dlv1");
    victim->move(room);
    victim->delete_temp("client/loading");
    g_user_list[victim->object_id()] = victim;

    int before = me->sent;
    EXPECT_EQ(1, CallCmd(me, "downloadobject", victim->object_id(), (char*)""));
    EXPECT_GT(me->sent, before) << "物件详情下行到观看者";
    EXPECT_TRUE(me->has("dlv1")) << "封包携带被查看者身份";

    victim->move(nullptr);
    g_user_list.erase(victim->object_id());
    delete victim;
}

TEST_F(CmdUserSkillCmdTest, DownloadObject_UnknownId_StaysSilent)
{
    int before = me->sent;
    EXPECT_EQ(1, CallCmd(me, "downloadobject", 999999, (char*)""));
    EXPECT_EQ(before, me->sent) << "查无此物不得有下行";
}

TEST_F(CmdUserSkillCmdTest, Downloaduser_KnownUser_Downloads)
{
    auto* victim = new StateRecordingUser();
    victim->set_name("被查看者", "dlu1");
    g_user_list[victim->object_id()] = victim;

    EXPECT_EQ(1, CallCmd(me, "downloaduser", victim->object_id(), (char*)""));
    EXPECT_TRUE(me->has("dlu1")) << "玩家信息封包带身份";

    g_user_list.erase(victim->object_id());
    delete victim;
}

TEST_F(CmdUserSkillCmdTest, Downloaduser_Unknown_ReturnsZero)
{
    EXPECT_EQ(0, CallCmd(me, "downloaduser", 999999, (char*)""));
}

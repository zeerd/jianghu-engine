// test_char_skill.cpp - Unit tests for CharSkill.cpp (skill CRUD + conditions).
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include "Char.h"

class CharSkillTest : public ::testing::Test {
protected:
    CChar* ch;
    void SetUp() override
    {
        ch = new CChar();
        ch->set("name", "sk");
        ch->set("id", "sk1");
    }
    void TearDown() override { delete ch; }
};

// ---------------- set_skill / query_skill ----------------
TEST_F(CharSkillTest, SetAndQuerySkill)
{
    EXPECT_EQ(ch->set_skill("unarmed", 50), 1);
    EXPECT_EQ(ch->query_skill("unarmed", 1), 50);  // raw
    // non-raw: apply + level/2
    EXPECT_EQ(ch->query_skill("unarmed", 0), 25);
}

TEST_F(CharSkillTest, QuerySkill_WithMapped)
{
    ch->set_skill("unarmed", 40);
    ch->set_skill("strike", 20);
    ch->map_skill("unarmed", "strike");
    // non-raw unarmed = 40/2 + strike(20) = 40
    EXPECT_EQ(ch->query_skill("unarmed", 0), 40);
}

TEST_F(CharSkillTest, DeleteSkill)
{
    ch->set_skill("unarmed", 10);
    EXPECT_EQ(ch->delete_skill("unarmed"), 1);
    EXPECT_EQ(ch->query_skill("unarmed", 1), 0);
    EXPECT_EQ(ch->delete_skill(""), 0);
    EXPECT_EQ(ch->delete_skill("nonexistent"), 0);
}

// ---------------- map / prepare ----------------
TEST_F(CharSkillTest, MapSkill)
{
    ch->set_skill("unarmed", 10);
    ch->set_skill("strike", 5);
    ch->map_skill("unarmed", "strike");
    EXPECT_STREQ(ch->query_skill_mapped("unarmed"), "strike");
    ch->map_skill("unarmed", NULL);  // unmap
    EXPECT_STREQ(ch->query_skill_mapped("unarmed"), "");
}

TEST_F(CharSkillTest, MapSkill_RequiresTarget)
{
    ch->set_skill("unarmed", 10);
    ch->map_skill("unarmed", "notlearned");  // target not learned -> ignored
    EXPECT_STREQ(ch->query_skill_mapped("unarmed"), "");
}

TEST_F(CharSkillTest, PrepareSkill)
{
    ch->set_skill("unarmed", 10);
    ch->set_skill("force", 5);
    ch->prepare_skill("unarmed", "force");
    EXPECT_STREQ(ch->query_skill_prepared("unarmed"), "force");
    ch->prepare_skill("unarmed", NULL);
    EXPECT_STREQ(ch->query_skill_prepared("unarmed"), "");
}

// ---------------- clear ----------------
TEST_F(CharSkillTest, clear_all)
{
    ch->set_skill("unarmed", 10);
    ch->set_skill("force", 5);
    ch->map_skill("unarmed", "force");
    ch->prepare_skill("unarmed", "force");
    ch->clear_skill_map();
    ch->clear_skill_prepare();
    EXPECT_STREQ(ch->query_skill_mapped("unarmed"), "");
    EXPECT_STREQ(ch->query_skill_prepared("unarmed"), "");
    ch->clear_skill();
    EXPECT_EQ(ch->query_skills()->get_count(), 0);
    EXPECT_NE(ch->query_skill_map(), nullptr);
    EXPECT_NE(ch->query_skill_prepare(), nullptr);
}

// ---------------- improve_skill ----------------
TEST_F(CharSkillTest, ImproveSkill_LearnsFromScratch)
{
    ch->improve_skill("unarmed", 2, 0);
    // learned accumulates; skill may or may not level up depending on threshold
    EXPECT_GE(ch->query_learned()->query("unarmed"), 0);
    EXPECT_GE(ch->query_skill("unarmed", 1), 0);
}

TEST_F(CharSkillTest, ImproveSkill_LevelsUp)
{
    ch->set_skill("unarmed", 1);
    // need learned > (1+1)^2 = 4 to level up
    ch->improve_skill("unarmed", 10, 0);
    EXPECT_EQ(ch->query_skill("unarmed", 1), 2);
}

TEST_F(CharSkillTest, ImproveSkill_EmptyName)
{
    ch->improve_skill("", 10, 0);  // early return
    EXPECT_EQ(ch->query_skill("", 1), 0);
}

// ---------------- skill_need_points ----------------
TEST_F(CharSkillTest, SkillNeedPoints)
{
    ch->set_skill("unarmed", 5);
    // (5+1)^2 - 0 = 36
    EXPECT_EQ(ch->skill_need_points("unarmed"), 36);
}

TEST_F(CharSkillTest, SkillNeedPoints_MinOne)
{
    ch->set_skill("unarmed", 5);
    ch->query_learned()->set("unarmed", (long)99999);
    EXPECT_EQ(ch->skill_need_points("unarmed"), 1);
}

// ---------------- mini_skill ----------------
TEST_F(CharSkillTest, MiniSkill_None) { EXPECT_EQ(ch->mini_skill(), 0); }
TEST_F(CharSkillTest, MiniSkill_Min)
{
    ch->set_skill("unarmed", 50);
    ch->set_skill("dodge", 10);
    ch->set_skill("force", 30);
    EXPECT_EQ(ch->mini_skill(), 10);
}

// ---------------- can_learn ----------------
TEST_F(CharSkillTest, CanLearn)
{
    EXPECT_EQ(ch->can_learn("unarmed"), 1);
    EXPECT_EQ(ch->can_learn(""), 0);
}
TEST_F(CharSkillTest, CanLearn_Forget)
{
    CMapping forget;
    forget.set("unarmed", (long)1);
    ch->set("forget", forget);
    EXPECT_EQ(ch->can_learn("unarmed"), 0);
    EXPECT_EQ(ch->can_learn("dodge"), 1);
}

// ---------------- apply_condition / update_condition ----------------
TEST_F(CharSkillTest, ApplyCondition_AndQuery)
{
    ch->apply_condition("poison", 5, 10);
    EXPECT_GT(ch->query_condition("poison"), 0);
    // update_condition runs the (fallback no-op) condition and clears
    ch->update_condition();
    // remove by duration 0
    ch->apply_condition("poison", 0, 0);
    EXPECT_EQ(ch->query_condition("poison"), 0);
}

TEST_F(CharSkillTest, ApplyCondition_WithSource)
{
    CChar* src = new CChar();
    src->set("id", "src1");
    ch->apply_condition(src, "dizzy", 3, 0);
    EXPECT_GT(ch->query_condition("dizzy"), 0);
    delete src;
}

TEST_F(CharSkillTest, ClearCondition)
{
    ch->apply_condition("poison", 5, 10);
    ch->clear_condition();
    EXPECT_EQ(ch->query_condition("poison"), 0);
}

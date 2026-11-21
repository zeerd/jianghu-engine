// test_data_source_types.cpp - 数据域类型契约（槽位域族 / 有界域族）
//
// 构建/运行：.\run-unit-tests.ps1（仓库根）
//
// 槽位值的宽度是64 位升级的裁定结果：无上界的数据域跟随 mapping_value_t
// 一路走到底，有界域（等级、序号、句柄）固定 int32。本文件把这条分界钉死——
// 日后有人把某一域悄悄改窄、或把有界域误加宽，这里先红。
#include "pch.h"

#include <type_traits>
#include <utility>

#include "Char.h"
#include "Container.h"
#include "Mapping.h"

// ---------------------------------------------------------------------
// 有界域族：设计上有明确上界，或是下标/句柄/枚举，固定 4 字节
// ---------------------------------------------------------------------
TEST(DataSourceTypes, BoundedDomainsStayInt32)
{
    EXPECT_EQ(sizeof(level_t), 4u);         // 等级：NORMAL_MAX_LEVEL 封顶
    EXPECT_EQ(sizeof(skill_level_t), 4u);   // 技能等级：对照表域
    EXPECT_EQ(sizeof(generation_t), 4u);    // 门派世代
    EXPECT_EQ(sizeof(team_index_t), 4u);    // 队伍序号：FIGHT_TEAM_A/B 下标
    EXPECT_EQ(sizeof(flag_t), 4u);          // 开关标记：0/1 语义
    EXPECT_EQ(sizeof(wizard_level_t), 4u);  // 巫师权限
    EXPECT_EQ(sizeof(object_id_t), 4u);     // 对象 id：句柄表键
    EXPECT_EQ(sizeof(ratio_t), 4u);         // 百分比比率
}

// ---------------------------------------------------------------------
// 槽位域族：量级无上界，宽度必须与槽位同源，绝不回退成 32 位
// ---------------------------------------------------------------------
TEST(DataSourceTypes, SlotDomainsFollowMappingValue)
{
    EXPECT_EQ(sizeof(exp_t), sizeof(mapping_value_t));        // 经验
    EXPECT_EQ(sizeof(money_t), sizeof(mapping_value_t));      // 金钱
    EXPECT_EQ(sizeof(hp_t), sizeof(mapping_value_t));         // 气血/法力
    EXPECT_EQ(sizeof(damage_t), sizeof(mapping_value_t));     // 伤害与治疗量
    EXPECT_EQ(sizeof(timestamp_t), sizeof(mapping_value_t));  // 时间戳
    EXPECT_EQ(sizeof(amount_t), sizeof(mapping_value_t));     // 数量与重量
    EXPECT_EQ(sizeof(count_t), sizeof(mapping_value_t));      // 各类计数
}

// ---------------------------------------------------------------------
// 槽位往返：读出的值不得被截断（64 位构建下覆盖 int32 以上的量级）
// ---------------------------------------------------------------------
TEST(DataSourceTypes, SlotRoundTripKeepsValue)
{
    CMapping m;

    const mapping_value_t fits32 = static_cast<mapping_value_t>(2000000000);
    m.set("k", fits32);
    EXPECT_EQ(m.query("k"), fits32);

    if (sizeof(mapping_value_t) >= 8) {
        // 64 位构建：超过 int32 上限的值必须原样读回
        const mapping_value_t beyond32
            = static_cast<mapping_value_t>(5000000000LL);
        m.set("k", beyond32);
        EXPECT_EQ(m.query("k"), beyond32);
    }
}

// ---------------------------------------------------------------------
// 接口契约：域 API 的入参/出参类型写在签名里，改签名即编译失败
// ---------------------------------------------------------------------
namespace {

using std::declval;

static_assert(std::is_same<decltype(CChar::level2_exp(0)), exp_t>::value,
              "level2_exp 返回经验，必须是 exp_t");
static_assert(std::is_same<decltype(CChar::exp2level(0)), level_t>::value,
              "exp2level 返回等级，必须是 level_t");
static_assert(
    std::is_same<decltype(CChar::level2skill(0)), skill_level_t>::value,
    "level2skill 返回技能点上限，必须是 skill_level_t");

static_assert(
    std::is_same<decltype(declval<CChar&>().query_valid_exp()), exp_t>::value,
    "可用经验必须是 exp_t");
static_assert(std::is_same<decltype(declval<CChar&>().query_skill("x")),
                           skill_level_t>::value,
              "技能等级必须是 skill_level_t");
static_assert(
    std::is_same<decltype(declval<CChar&>().receive_damage("hp", 0, nullptr)),
                 damage_t>::value,
    "伤害入口与出口必须是 damage_t");
static_assert(std::is_same<decltype(declval<CContainer&>().query_amount()),
                           amount_t>::value,
              "数量查询必须是 amount_t");
static_assert(std::is_same<decltype(declval<CContainer&>().query_weight()),
                           amount_t>::value,
              "重量查询必须是 amount_t");

}  // namespace

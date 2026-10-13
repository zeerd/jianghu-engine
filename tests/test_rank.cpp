// test_rank.cpp — Rank.cpp 称谓查询函数的分支矩阵测试。
//
// 契约：query_rank / query_respect / query_rude / query_self /
//   query_self_rude / query_close / query_self_close / query_waimao
//   在 Lua 化前后返回完全一致的称谓文本。本文件按源码分支逐条锁定返回值，
//   迁移后同一批断言必须原样通过（C++ 为 oracle）。
//
// Build+Run: .\run-coverage.ps1
#include "pch.h"

#include <string>

#include "Char.h"
#include "Rank.h"
#include "test_lua_env.h"

namespace {

class RankTest : public ::testing::Test {
protected:
    CChar* ob;

    void SetUp() override
    {
        // 称谓算法已 Lua 化：query_* 经 hooks.rank_rule 派发，需真实 Lua 环境。
        // 迁移前该环境对纯 C++ 算法无害，迁移后为必需（前后同一套断言）。
        ASSERT_NE(ensure_lua_env(), nullptr);
        ob = new CChar();
        ob->set("id", "rk");
        ob->set("name", "称谓测试");
    }
    void TearDown() override { delete ob; }

    // 重新构造一个干净角色，避免用例间字段污染
    void Fresh()
    {
        delete ob;
        ob = new CChar();
        ob->set("id", "rk");
        ob->set("name", "称谓测试");
    }

    // 少林派：query_rank 的 buddhism 阶梯需要 family 映射存在且
    // family_name=少林派
    void MakeShaolin(int buddhism)
    {
        ob->set("family/family_name", "少林派");
        ob->query_skills()->set("buddhism", (long)buddhism);
    }
};

// ---------------- query_rank：女性 ----------------
TEST_F(RankTest, RankFemaleWeiwang)
{
    ob->set("gender", "女性");
    ob->set("weiwang", (long)100);  // 女性 weiwang >= 100
    EXPECT_STREQ("【绝世佳人】", query_rank(ob));
}

TEST_F(RankTest, RankFemaleScoreTiers)
{
    ob->set("gender", "女性");
    ob->set("weiwang", (long)0);
    ob->set("trade/score", (long)600);
    EXPECT_STREQ("【 小  贩 】", query_rank(ob));
    ob->set("trade/score", (long)3000);
    EXPECT_STREQ("【 商  人 】", query_rank(ob));
    ob->set("trade/score", (long)20000);
    EXPECT_STREQ("【大 商 人】", query_rank(ob));
    ob->set("trade/score", (long)60000);
    EXPECT_STREQ("【红顶商人】", query_rank(ob));
}

TEST_F(RankTest, RankFemaleClassTitles)
{
    struct {
        const char* cla;
        const char* want;
    } cases[] = {
        {"bonze", "【 尼  姑 】"},     {"taoist", "【 女  冠 】"},
        {"bandit", "【 女飞贼 】"},    {"dancer", "【 舞  姬 】"},
        {"scholar", "【 才  女 】"},   {"officer", "【 女  官 】"},
        {"fighter", "【 女武者 】"},   {"swordsman", "【 女剑士 】"},
        {"alchemist", "【 女方士 】"}, {"shaman", "【 女巫医 】"},
        {"beggar", "【女叫化子】"},
    };
    for (auto& c : cases) {
        Fresh();
        ob->set("gender", "女性");
        ob->set("class", c.cla);
        EXPECT_STREQ(c.want, query_rank(ob)) << "class=" << c.cla;
    }
}

TEST_F(RankTest, RankFemaleElseBranches)
{
    // pks > 150
    Fresh();
    ob->set("gender", "女性");
    ob->set("pks", (long)151);
    EXPECT_STREQ("【女杀人魔】", query_rank(ob));

    // repute 阶梯
    struct {
        long shen;
        const char* want;
    } tiers[] = {
        {500000, "【旷世女侠】"},  {50000, "【 女大侠 】"},
        {5000, "【 女  侠 】"},    {500, "【 女少侠 】"},
        {-500000, "【 女魔王 】"}, {-50000, "【 女大魔 】"},
        {-5000, "【 女  魔 】"},   {-500, "【 女少魔 】"},
    };
    for (auto& t : tiers) {
        Fresh();
        ob->set("gender", "女性");
        ob->set("repute", (long)t.shen);
        EXPECT_STREQ(t.want, query_rank(ob)) << "shen=" << t.shen;
    }

    // 惯窃：thief > query_skill("stealing") + 10（stealing 缺省 0 → thief>10）
    Fresh();
    ob->set("gender", "女性");
    ob->set("thief", (long)11);
    EXPECT_STREQ("【女 惯 窃】", query_rank(ob));

    // 平民
    Fresh();
    ob->set("gender", "女性");
    EXPECT_STREQ("【 平  民 】", query_rank(ob));
}

// ---------------- query_rank：男性 ----------------
TEST_F(RankTest, RankMaleWeiwang)
{
    ob->set("gender", "男性");
    ob->set("weiwang", (long)101);  // 男性 weiwang > 100
    EXPECT_STREQ("【盖世英豪】", query_rank(ob));
    // 恰为 100 不触发（>100 严格），落到平民
    ob->set("weiwang", (long)100);
    EXPECT_STREQ("【 平  民 】", query_rank(ob));
}

TEST_F(RankTest, RankMaleScoreTiers)
{
    ob->set("gender", "男性");
    ob->set("trade/score", (long)600);
    EXPECT_STREQ("【 小  贩 】", query_rank(ob));
    ob->set("trade/score", (long)3000);
    EXPECT_STREQ("【 商  人 】", query_rank(ob));
    ob->set("trade/score", (long)20000);
    EXPECT_STREQ("【大 商 人】", query_rank(ob));
    ob->set("trade/score", (long)60000);
    EXPECT_STREQ("【红顶商人】", query_rank(ob));
}

TEST_F(RankTest, RankMaleBonzeBuddhaTiers)
{
    struct {
        int budd;
        const char* want;
    } tiers[] = {
        {150, "【 长  老 】"}, {120, "【 圣  僧 】"}, {90, "【 罗  汉 】"},
        {60, "【 尊  者 】"},  {40, "【 禅  师 】"},  {30, "【 比  丘 】"},
        {0, "【 僧  人 】"},
    };
    for (auto& t : tiers) {
        Fresh();
        ob->set("gender", "男性");
        ob->set("class", "bonze");
        MakeShaolin(t.budd);
        EXPECT_STREQ(t.want, query_rank(ob)) << "budd=" << t.budd;
    }
    // 非少林派僧人：budd=0 → 僧人
    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    EXPECT_STREQ("【 僧  人 】", query_rank(ob));
}

TEST_F(RankTest, RankMaleClassTitles)
{
    struct {
        const char* cla;
        const char* want;
    } cases[] = {
        {"taoist", "【 道  士 】"},    {"bandit", "【 盗  贼 】"},
        {"scholar", "【 书  生 】"},   {"officer", "【 官  差 】"},
        {"fighter", "【 武  者 】"},   {"swordsman", "【 剑  士 】"},
        {"alchemist", "【 方  士 】"}, {"shaman", "【 巫  医 】"},
        {"beggar", "【 叫化子 】"},
    };
    for (auto& c : cases) {
        Fresh();
        ob->set("gender", "男性");
        ob->set("class", c.cla);
        EXPECT_STREQ(c.want, query_rank(ob)) << "class=" << c.cla;
    }
}

TEST_F(RankTest, RankMaleElseBranches)
{
    Fresh();
    ob->set("gender", "男性");
    ob->set("pks", (long)151);
    EXPECT_STREQ("【 杀人魔 】", query_rank(ob));

    struct {
        long shen;
        const char* want;
    } tiers[] = {
        {500000, "【旷世大侠】"},  {50000, "【 大  侠 】"},
        {5000, "【 侠  客 】"},    {500, "【 少  侠 】"},
        {-500000, "【 魔  王 】"}, {-50000, "【 大  魔 】"},
        {-5000, "【 魔  头 】"},   {-500, "【 少  魔 】"},
    };
    for (auto& t : tiers) {
        Fresh();
        ob->set("gender", "男性");
        ob->set("repute", (long)t.shen);
        EXPECT_STREQ(t.want, query_rank(ob)) << "shen=" << t.shen;
    }

    Fresh();
    ob->set("gender", "男性");
    ob->set("thief", (long)11);
    EXPECT_STREQ("【 惯  窃 】", query_rank(ob));

    Fresh();
    ob->set("gender", "男性");
    EXPECT_STREQ("【 平  民 】", query_rank(ob));
}

// ---------------- query_respect ----------------
TEST_F(RankTest, RespectOverride)
{
    ob->set("rank_info/respect", "欧冶师傅");
    EXPECT_STREQ("欧冶师傅", query_respect(ob));
}

TEST_F(RankTest, RespectFemale)
{
    struct {
        const char* cla;
        long age;
        const char* want;
    } cases[] = {
        {"bonze", 17, "小师太"},  {"bonze", 18, "师太"},
        {"taoist", 17, "小仙姑"}, {"taoist", 18, "仙姑"},
        {"", 17, "小姑娘"},       {"", 18, "姑娘"},
        {"", 50, "婆婆"},
    };
    for (auto& c : cases) {
        Fresh();
        ob->set("gender", "女性");
        ob->set("class", c.cla);
        ob->set("age", (long)c.age);
        EXPECT_STREQ(c.want, query_respect(ob)) << c.cla << "/" << c.age;
    }
}

TEST_F(RankTest, RespectMale)
{
    struct {
        const char* cla;
        long age;
        const char* want;
    } cases[] = {
        {"bonze", 17, "小师父"},   {"bonze", 18, "大师"},
        {"taoist", 17, "道兄"},    {"taoist", 18, "道长"},
        {"fighter", 17, "小老弟"}, {"fighter", 18, "壮士"},
        {"fighter", 50, "老前辈"}, {"swordsman", 17, "小老弟"},
        {"", 19, "小兄弟"},        {"", 20, "壮士"},
        {"", 50, "老爷子"},
    };
    for (auto& c : cases) {
        Fresh();
        ob->set("gender", "男性");
        ob->set("class", c.cla);
        ob->set("age", (long)c.age);
        EXPECT_STREQ(c.want, query_respect(ob)) << c.cla << "/" << c.age;
    }
}

// ---------------- query_rude ----------------
TEST_F(RankTest, RudeOverride)
{
    ob->set("rank_info/rude", "阉贼");
    EXPECT_STREQ("阉贼", query_rude(ob));
}

TEST_F(RankTest, RudeFemale)
{
    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "bonze");
    EXPECT_STREQ("贼尼", query_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "taoist");
    EXPECT_STREQ("妖女", query_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)29);
    EXPECT_STREQ("小贱人", query_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)30);
    EXPECT_STREQ("死老太婆", query_rude(ob));
}

TEST_F(RankTest, RudeMale)
{
    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)49);
    EXPECT_STREQ("死秃驴", query_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)50);
    EXPECT_STREQ("老秃驴", query_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "taoist");
    EXPECT_STREQ("死牛鼻子", query_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)19);
    EXPECT_STREQ("小王八蛋", query_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)20);
    EXPECT_STREQ("臭贼", query_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)50);
    EXPECT_STREQ("老匹夫", query_rude(ob));
}

// ---------------- query_self ----------------
TEST_F(RankTest, SelfOverride)
{
    ob->set("rank_info/self", "在下某");
    EXPECT_STREQ("在下某", query_self(ob));
}

TEST_F(RankTest, SelfFemale)
{
    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "bonze");
    ob->set("age", (long)49);
    EXPECT_STREQ("贫尼", query_self(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "bonze");
    ob->set("age", (long)50);
    EXPECT_STREQ("老尼", query_self(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)29);
    EXPECT_STREQ("小女子", query_self(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)30);
    EXPECT_STREQ("妾身", query_self(ob));
}

TEST_F(RankTest, SelfMale)
{
    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)49);
    EXPECT_STREQ("贫僧", query_self(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)50);
    EXPECT_STREQ("老纳", query_self(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "taoist");
    EXPECT_STREQ("贫道", query_self(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)49);
    EXPECT_STREQ("在下", query_self(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)50);
    EXPECT_STREQ("老头子", query_self(ob));
}

// ---------------- query_self_rude ----------------
TEST_F(RankTest, SelfRudeOverride)
{
    ob->set("rank_info/self_rude", "本姑奶奶");
    EXPECT_STREQ("本姑奶奶", query_self_rude(ob));
}

TEST_F(RankTest, SelfRudeFemale)
{
    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "bonze");
    ob->set("age", (long)49);
    EXPECT_STREQ("贫尼", query_self_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("class", "bonze");
    ob->set("age", (long)50);
    EXPECT_STREQ("老尼", query_self_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)29);
    EXPECT_STREQ("本姑娘", query_self_rude(ob));

    Fresh();
    ob->set("gender", "女性");
    ob->set("age", (long)30);
    EXPECT_STREQ("老娘", query_self_rude(ob));
}

TEST_F(RankTest, SelfRudeMale)
{
    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)49);
    EXPECT_STREQ("大和尚我", query_self_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "bonze");
    ob->set("age", (long)50);
    EXPECT_STREQ("老和尚我", query_self_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("class", "taoist");
    EXPECT_STREQ("本山人", query_self_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)49);
    EXPECT_STREQ("大爷我", query_self_rude(ob));

    Fresh();
    ob->set("gender", "男性");
    ob->set("age", (long)50);
    EXPECT_STREQ("老子", query_self_rude(ob));
}

// ---------------- query_close ----------------
TEST_F(RankTest, CloseNoTarget)
{
    ob->set("age", (long)49);
    EXPECT_STREQ("弟弟", query_close(ob, nullptr));
    ob->set("age", (long)50);
    EXPECT_STREQ("哥哥", query_close(ob, nullptr));
}

TEST_F(RankTest, CloseNpcByAge)
{
    CChar other;
    // 对方无 mud_age → 用 age 比较
    other.set("age", (long)30);
    other.set("gender", "女性");

    ob->set("age", (long)30);
    EXPECT_STREQ("妹妹", query_close(ob, &other));  // a1>=a2 女性
    ob->set("age", (long)20);
    EXPECT_STREQ("姐姐", query_close(ob, &other));  // a1<a2 女性

    other.set("gender", "男性");
    ob->set("age", (long)30);
    EXPECT_STREQ("弟弟", query_close(ob, &other));
    ob->set("age", (long)20);
    EXPECT_STREQ("哥哥", query_close(ob, &other));
}

TEST_F(RankTest, ClosePlayerByMudAge)
{
    CChar other;
    other.set("mud_age", (long)100);  // 对方有 mud_age → 玩家，用 mud_age 比
    other.set("gender", "男性");
    ob->set("mud_age", (long)100);
    EXPECT_STREQ("弟弟", query_close(ob, &other));  // a1>=a2
    ob->set("mud_age", (long)50);
    EXPECT_STREQ("哥哥", query_close(ob, &other));  // a1<a2
}

// ---------------- query_self_close ----------------
TEST_F(RankTest, SelfCloseFemaleMudAge)
{
    CChar other;
    other.set("mud_age", (long)100);
    ob->set("gender", "女性");
    ob->set("mud_age", (long)100);
    EXPECT_STREQ("姐姐我", query_self_close(ob, &other));
    ob->set("mud_age", (long)50);
    EXPECT_STREQ("小妹我", query_self_close(ob, &other));
}

TEST_F(RankTest, SelfCloseMaleByAge)
{
    CChar other;
    other.set("age", (long)30);  // 无 mud_age → 用 age
    ob->set("gender", "男性");
    ob->set("age", (long)30);
    EXPECT_STREQ("愚兄我", query_self_close(ob, &other));
    ob->set("age", (long)20);
    EXPECT_STREQ("小弟我", query_self_close(ob, &other));
}

TEST_F(RankTest, SelfCloseNoTarget)
{
    ob->set("gender", "男性");
    ob->set("age", (long)10);  // a1>=0
    EXPECT_STREQ("愚兄我", query_self_close(ob, nullptr));
}

// ---------------- query_waimao ----------------
TEST_F(RankTest, WaimaoMale)
{
    const char* m[41] = {
        /*0-10 走 default*/
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        "生得呲牙咧嘴，黑如锅底，奇丑无比。",
        "生得面如桔皮，头肿如猪，让人不想再看第二眼。",
        "生得贼眉鼠眼，身高三尺，宛若猴状。",
        "生得肥头大耳，腹圆如鼓，手脚短粗，令人发笑。",
        "生得面颊深陷，瘦如枯骨，让人要发恶梦。",
        "生得獐头鼠须，让人一看就不生好感。",
        "生得膀大腰圆，满脸横肉，恶形恶相。",
        "生得相貌平平，不会给人留下什么印相。",
        "生得腰圆背厚，面阔口方，骨格不凡。",
        "生得眉目清秀，端正大方，一表人才。",
        "生得鼻直口方，线条分明，显出刚毅性格。",
        "生得浓眉大眼，高大挺拔，气宇轩昂。",
        "生得肤色白皙，红唇墨发，斯文有礼。",
        "生得虎背熊腰，壮健有力，英姿勃发。",
        "生得眉如双剑，眼如明星，英挺出众。",
        "生得满面浓髯，环眼豹鼻，威风凛凛，让人倾倒。",
        "生得剑眉星目，英姿勃勃，仪表不凡。",
        "生得面若秋月，儒雅斯文，举止适度。",
        "生得目似点漆，高大挺俊，令人心动。",
        "生得英俊潇洒，风流倜傥。",
        "现在双眼光华莹润，透出摄人心魄的光芒。",
        "现在举动如行云游水，独蕴风情，吸引所有异性目光。",
        "现在双目如星，眉梢传情，所见者无不为之心动。",
        "现在粉面朱唇，身姿俊俏，举止风流无限。",
        "现在丰神如玉，目似朗星，令人过目难忘。",
        "现在面如美玉，粉妆玉琢，俊美不凡。",
        "现在飘逸出尘，潇洒绝伦。",
        "现在丰神俊朗，长身玉立，宛如玉树临风。",
        "现在神清气爽，骨格清奇，宛若仙人。",
    };
    for (int per = 11; per <= 39; per++)
        EXPECT_STREQ(m[per], query_waimao("男性", per)) << "per=" << per;

    EXPECT_STREQ("生得眉歪眼斜，瘌头癣脚，不象人样。",
                 query_waimao("男性", 10));
    EXPECT_STREQ("现在一派神人气度，仙风道骨，举止出尘。",
                 query_waimao("男性", 40));
    // 无性 与 男性 共用同一分支
    EXPECT_STREQ("现在神清气爽，骨格清奇，宛若仙人。",
                 query_waimao("无性", 39));
}

TEST_F(RankTest, WaimaoFemale)
{
    const char* f[41] = {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        "生得歪鼻斜眼，脸色灰败，直如鬼怪一般。",
        "生得八字眉，三角眼，鸡皮黄发，让人一见就想吐。",
        "生得眼小如豆，眉毛稀疏，手如猴爪，不成人样。",
        "生得一嘴大暴牙，让人一看就没好感。",
        "生得满脸疙瘩，皮色粗黑，丑陋不堪。",
        "生得干黄枯瘦，脸色腊黄，毫无女人味。",
        "生得身材瘦小，肌肤无光，两眼无神。",
        "生得虽不标致，倒也白净，有些动人之处。",
        "生得肌肤微丰，雅淡温宛，清新可人。",
        "生得风姿楚楚，明丽动人。",
        "生得如梨花带露，清新秀丽。",
        "生得巧笑嫣然，宛约可人。",
        "生得鲜艳妍媚，肌肤莹透，引人遐思。",
        "生得腮凝新荔，目若秋水，千娇百媚。",
        "生得粉嫩白至，如芍药笼烟，雾里看花。",
        "生得丰胸细腰，妖娆多姿，让人一看就心跳不已。",
        "生得削肩细腰，身材苗条，娇媚动人，顾盼神飞。",
        "生得鸭蛋秀脸，俊眼修眉，黑发如瀑，风情万种。",
        "生得娇小玲珑，宛如飞燕再世，楚楚动人。",
        "生得闲静如姣花照水，行动似弱柳扶风，体态万千。",
        "现在娇若春花，媚如秋月，真的能沉鱼落雁。。",
        "现在眉目如画，肌肤胜雪，真可谓闭月羞花。",
        "现在凤眼柳眉，粉面含春，丹唇贝齿，转盼多情。",
        "现在鬓若刀裁，眉如墨画，面如桃瓣，目若秋波。",
        "现在鲜艳妩媚，袅娜风流，柔媚姣俏，粉光脂艳。",
        "现在丰润嫩白，婴桃小口，眉目含情，仿佛太真重临。",
        "现在气质美如兰，才华馥比山，令人见之忘俗。",
        "现在灿若明霞，宝润如玉，恍如神妃仙子。",
        "现在美若天仙，不沾一丝烟尘。",
    };
    for (int per = 11; per <= 39; per++)
        EXPECT_STREQ(f[per], query_waimao("女性", per)) << "per=" << per;

    EXPECT_STREQ("现在宛如玉雕冰塑，似梦似幻，已不再是凡间人物",
                 query_waimao("女性", 40));
    EXPECT_STREQ("生得丑如无盐，状如夜叉，女人长成这样真是人生悲剧。",
                 query_waimao("女性", 10));
}

TEST_F(RankTest, WaimaoUnknownGender)
{
    EXPECT_STREQ("", query_waimao("妖怪", 20));
}

}  // namespace

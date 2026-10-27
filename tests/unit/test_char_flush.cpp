// test_char_flush.cpp - CharFlush.cpp（客户端信息同步）协议契约测试
//
// 每个用例断言可观察的协议包或传输缓冲，不存在 EXPECT_TRUE(true) 占位：
//  - StateRecordingUser 记录 send_command 入参 → 断言 flush 拼装的协议内容；
//  - TransportProbeUser 暴露真实 CUser 发送缓冲 → 断言
//  buffered/立即/早退投递路径。
//
// 关键契约（生产 owner: src/char/CharFlush.cpp）：
//  1. flush_my_info/flush_my_equip 有 has_client 守卫（投递能力而非身份，
//     net_dead 掉线同样拦下）；flush_my_skill 无守卫，能力在收件人的
//     send_command。
//  2. flush_my_info 的属性字母 ↔ 属性映射（szA/szAttr 平行数组）与
//     skill_type 三选一（武器 > prepare[0] > unarmed）。
//  3. flush_my_equip 每 10 条分包（i%10==0），equipped 标志紧随同索引 I 条目。
//  4. flush_my_skill 包发给收件人 me（T= 是 subject 的 object_id），
//     P= 仅 this==me，B=1/E=1 框架位固定首/末包，M=/K=1 映射/装备标记。
//  5. send_table 的 st=1/2/3 行框架，表头 T/R/P 仅随首行，空表 0 包。
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"

#include <string>
#include <vector>

#include "Char.h"
#include "User.h"
#include "Item.h"
#include "Mapping.h"
#include "Global.h"      // RTNINFO/RTNITEM/SENDTABLE 协议枚举、装备位
#include "Skill.h"       // SKILL_USAGE_*
#include "CombatCalc.h"  // skill_power（I=/K= 期望值 oracle）
#include "test_state_recorder.h"
#include "test_lua_env.h"

namespace {

// flush_my_info 的攻防协议算式（CharFlush.cpp: I=att&K=def）。
// skill_power 是协作者 oracle（CombatCalc 独立单元），/100+1 与武器分支
// 是协议定义（客户端按此显示攻防）。
unsigned long expected_att(CChar* ch, const char* skill_type)
{
    double ap = CCombatCalc::skill_power(ch, skill_type, SKILL_USAGE_ATTACK);
    unsigned long val = (unsigned long)ap;
    if (val > 4200000000UL) val = 4200000000UL;
    return val / 100 + 1;
}

unsigned long expected_def(CChar* ch, const char* skill_type, bool has_weapon)
{
    double dp = CCombatCalc::skill_power(ch, "dodge", SKILL_USAGE_DEFENSE);
    double pp = CCombatCalc::skill_power(ch, skill_type, SKILL_USAGE_DEFENSE);
    unsigned long d = (unsigned long)dp;
    unsigned long p = (unsigned long)pp;
    if (d > 4200000000UL) d = 4200000000UL;
    if (p > 4200000000UL) p = 4200000000UL;
    return d / 100 + (has_weapon ? p : p / 10) / 100 + 1;
}

// I=/J=/K=/L= 攻防段（fixture 固定 apply/damage=7、apply/armor=9）
std::string make_att_def_seg(unsigned long att, unsigned long def)
{
    char buf[128];
    snprintf(buf, static_cast<int>(sizeof(buf)), "I=%lu&J=7&K=%lu&L=9&", att,
             def);
    return std::string(buf);
}

// 从协议包中取 "&key=value&" 的 value（key 不存在返回空串）
std::string field(const std::string& msg, const char* key)
{
    char buf[256];
    get_value(msg.c_str(), key, buf, static_cast<int>(sizeof(buf)));
    return std::string(buf);
}

// 真实 CUser::send_command 的传输探针：覆写 send_off 计数、暴露发送缓冲。
// 覆盖 buffered 追加、缓冲合并、wait=0 立即投递、无客户端早退四条路径。
// 仅为测试子类，不引入生产 seam（send_off 本就是 virtual）。
class TransportProbeUser : public CUser {
public:
    void send_off() override
    {
        ++m_send_off_count;
        CUser::send_off();
    }
    int send_off_count() const { return m_send_off_count; }
    int buffered_len() const { return m_buffer_len; }

private:
    int m_send_off_count = 0;
};

}  // namespace

class CharFlushTest : public ::testing::Test {
protected:
    StateRecordingUser* u;

    void SetUp() override
    {
        ensure_lua_env();
        u = new StateRecordingUser();
        // 身份段（o/p/P/q/r/s/t）
        u->set("id", "fl1");
        u->set("name", "玩家");
        u->set("gender", "男性");
        u->set("class", "剑客");
        u->set("race", "人类");
        u->set("title", "百姓");
        u->set("icon", (long)9);
        // 属性段：相邻字母取值互不相同 → szA/szAttr 错位立即可见
        u->set("age", (long)30);
        u->set("level", (long)10);
        u->set("repute", (long)7);
        u->set("combat_exp", (long)10000);
        u->set("hp", (long)100);
        u->set("mp", (long)50);
        u->set("max_hp", (long)180);
        u->set("max_mp", (long)70);
        u->set("eff_hp", (long)95);
        u->set("pks", (long)2);
        u->set("mks", (long)5);
        u->set("str", (long)15);
        u->set("con", (long)15);
        u->set("int", (long)15);
        u->set("dex", (long)15);
        // J=/L= 透传字段
        u->set_temp("apply/damage", (long)7);
        u->set_temp("apply/armor", (long)9);
        // 技能：flush_my_skill 的 S=/L= 段与 skill_power 输入
        u->set_skill("unarmed", 50);
        u->set_skill("dodge", 40);
        u->set_skill("force", 30);
        // 物品 move() 的重量检查上限
        u->set_max_encumbrance(1000000);
        // 空连接：client 映射存在（has_client 为真）；serial=0 使真实投递
        // 路径的 (CGateInfo*) 判空为安全空操作（非 0 = 解引用假指针崩溃）。
        u->set_temp("client/serial", 0);
    }

    void TearDown() override { delete u; }
};

// ---------------- has_client 守卫 ----------------
// 覆盖守卫两个条件：未建立连接、net_dead 掉线，与已连接双向对照。
TEST_F(CharFlushTest, Flush_NoClient_NoPacket)
{
    StateRecordingUser* no_conn = new StateRecordingUser();

    // 从未建立连接：组装前被 has_client 拦下
    no_conn->flush_my_info();
    no_conn->flush_my_equip();
    EXPECT_EQ(no_conn->sent, 0);

    // 空连接建立：两个 flush 都产出
    no_conn->set_temp("client/serial", 0);
    no_conn->flush_my_info();
    EXPECT_GT(no_conn->sent, 0);
    no_conn->clear();
    no_conn->flush_my_equip();
    EXPECT_GT(no_conn->sent, 0);

    // 掉线（net_dead）：has_client 第二个条件拦下
    no_conn->set_temp("net_dead", (long)1);
    no_conn->clear();
    no_conn->flush_my_info();
    no_conn->flush_my_equip();
    EXPECT_EQ(no_conn->sent, 0);

    // 恢复在线：恢复产出
    no_conn->delete_temp("net_dead");
    no_conn->clear();
    no_conn->flush_my_info();
    EXPECT_GT(no_conn->sent, 0);
    delete no_conn;

    // fixture（已连接）同样产出，与 no_conn 形成对照
    u->flush_my_info();
    u->flush_my_equip();
    EXPECT_GT(u->sent, 0);
}

// ---------------- flush_my_info 协议内容 ----------------
// 单包、身份段、属性字母映射（szA/szAttr 平行数组）、E/F/G/H 攻防段。
// 期望值 oracle：query_str/con/int/dex（访问器，非被测函数）、
// query_valid_exp、level2_exp、CCombatCalc::skill_power。
TEST_F(CharFlushTest, FlushMyInfo_PacketContract)
{
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    const std::string& msg = u->msgs[0];

    // 指令头 + 身份段
    std::string head = "&C=" + std::to_string(static_cast<int>(RTNINFO))
                       + "&T=" + std::to_string(u->object_id())
                       + "&o=fl1&p=玩家&P=9&q=男性&r=剑客&s=人类&t=百姓&";
    EXPECT_EQ(msg.compare(0, head.size(), head), 0) << msg;

    // u=/v= 负重段（oracle）
    char encumb_seg[64];
    snprintf(encumb_seg, static_cast<int>(sizeof(encumb_seg)), "&u=%d&v=%d&",
             static_cast<int>(u->query_encumb()),
             static_cast<int>(u->query_max_encumbrance()));
    EXPECT_NE(msg.find(encumb_seg), std::string::npos) << msg;

    // exp/nex 段（oracle：query_valid_exp、level2_exp）
    char exp_seg[96];
    snprintf(exp_seg, static_cast<int>(sizeof(exp_seg)), "&exp=%d&nex=%d&",
             static_cast<int>(u->query_valid_exp()),
             static_cast<int>(CChar::level2_exp(11) - CChar::level2_exp(10)));
    EXPECT_NE(msg.find(exp_seg), std::string::npos) << msg;
    EXPECT_NE(msg.find("&master=&"), std::string::npos) << msg;

    // 属性字母 ↔ 属性值映射（szA/szAttr 错位 → 本段必红）
    std::string attrs
        = "&a=30&b=10&c=7&d=10000&e=100&f=50&g=180&h=70&i=95&"
          "m=2&n=5&";
    EXPECT_NE(msg.find(attrs), std::string::npos) << msg;

    // E/F/G/H = str/con/int/dex（含技能加成，oracle 取访问器值）
    char efgh[160];
    snprintf(efgh, static_cast<int>(sizeof(efgh)), "E=%d&F=%d&G=%d&H=%d&",
             static_cast<int>(u->query_str()), static_cast<int>(u->query_con()),
             static_cast<int>(u->query_int()),
             static_cast<int>(u->query_dex()));
    EXPECT_NE(msg.find(efgh), std::string::npos) << msg;

    // I/J/K/L = 攻防段：无武器无 prepare → skill_type=unarmed
    std::string att_def = make_att_def_seg(expected_att(u, "unarmed"),
                                           expected_def(u, "unarmed", false));
    EXPECT_NE(msg.find(att_def), std::string::npos) << msg;
}

// ---------------- flush_my_info 的 sk 截断 ----------------
// sk = min(combat_exp/3 - sk_exp, valid_exp)，两分支各覆盖一次，
// ASSERT 守护分支条件成立（防止 fixture 漂移后用例假绿）。
TEST_F(CharFlushTest, FlushMyInfo_SkExpCap)
{
    int32_t level10_exp = CChar::level2_exp(10);
    ASSERT_GT(level10_exp, 0);

    // 分支A 不截断：sk_raw < valid_exp → 显示 sk_raw
    u->set("combat_exp", (long)(3 * level10_exp + 3000));
    u->set("sk_exp", (long)100);
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    int32_t sk_raw = (3 * level10_exp + 3000) / 3 - 100;  // = level10_exp + 900
    int32_t valid_a = (3 * level10_exp + 3000) - level10_exp - 100;
    ASSERT_LT(sk_raw, valid_a) << "分支A守护：fixture 必须走未截断分支";
    EXPECT_NE(u->msgs[0].find("&sk=" + std::to_string(sk_raw) + "&"),
              std::string::npos)
        << u->msgs[0];
    EXPECT_NE(u->msgs[0].find("&exp=" + std::to_string(valid_a) + "&"),
              std::string::npos)
        << u->msgs[0];

    // 分支B 截断：combat_exp 对齐 level2_exp → valid_exp=0，sk 截到 0
    u->set("combat_exp", (long)level10_exp);
    u->set("sk_exp", (long)0);
    u->clear();
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    int32_t sk_b = level10_exp / 3;
    ASSERT_GT(sk_b, 0) << "分支B守护：fixture 必须走截断分支";
    EXPECT_NE(u->msgs[0].find("&sk=0&"), std::string::npos) << u->msgs[0];
    EXPECT_NE(u->msgs[0].find("&exp=0&"), std::string::npos) << u->msgs[0];
}

// ---------------- flush_my_info 的 skill_type 三选一 ----------------
// 优先级：武器 > prepare[0] > unarmed。三分支 I=/K= 用 skill_power oracle。
TEST_F(CharFlushTest, FlushMyInfo_SkillTypeSelection)
{
    // 分支1 默认：无武器无 prepare → unarmed
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    unsigned long att_unarmed = expected_att(u, "unarmed");
    unsigned long def_unarmed = expected_def(u, "unarmed", false);
    EXPECT_NE(u->msgs[0].find(make_att_def_seg(att_unarmed, def_unarmed)),
              std::string::npos)
        << u->msgs[0];

    // 分支2 prepare 非空 → v[0]（单键，顺序确定）
    u->clear();
    u->query_skill_prepare()->set("claw", "x");
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    unsigned long att_claw = expected_att(u, "claw");
    unsigned long def_claw = expected_def(u, "claw", false);
    EXPECT_NE(u->msgs[0].find(make_att_def_seg(att_claw, def_claw)),
              std::string::npos)
        << u->msgs[0];
    // 分支必须真的换了技能（否则上面断言无区分力）
    EXPECT_NE(att_claw, att_unarmed);

    // 分支3 武器 → 武器的 skill_type，且 def 走武器公式（pp 不除 10）。
    // prepare 保持非空：验证武器优先于 prepare（分支顺序回归即红）。
    u->set_skill("sword", 300);
    CItem* sword = new CItem();
    sword->set_name("铁剑", "sword");
    sword->set("skill_type", "sword");
    sword->move(u);
    (*u->get_equip_list())[WIELD_RIGHT_HAND] = sword->object_id();
    u->clear();  // 隔离 move 期间的 ADDOBJECT 包
    u->flush_my_info();
    ASSERT_EQ(u->sent, 1);
    unsigned long att_sword = expected_att(u, "sword");
    unsigned long def_sword = expected_def(u, "sword", true);
    EXPECT_NE(u->msgs[0].find(make_att_def_seg(att_sword, def_sword)),
              std::string::npos)
        << u->msgs[0];
    EXPECT_NE(att_sword, att_unarmed);

    // 不 move(nullptr)（会触发 is_user(u)->save()）；CItem dtor 为空，
    // u 的列表在 TearDown 析构时不被遍历。
    delete sword;
}

// ---------------- flush_my_equip ----------------
TEST_F(CharFlushTest, FlushMyEquip_Empty_ExactPacket)
{
    u->flush_my_equip();
    ASSERT_EQ(u->sent, 1);
    std::string expect = "&C=" + std::to_string(static_cast<int>(RTNITEM))
                         + "&B=0&T=" + std::to_string(u->object_id()) + "&";
    EXPECT_EQ(u->msgs[0], expect);
}

// 11 条触发 i%10==0 分包：包1 B=0 含 I0..I9，包2 B=10 含 I10。
// 索引按 get_item_list()（头插）推导，不假设 move 顺序。
TEST_F(CharFlushTest, FlushMyEquip_BatchAtTen)
{
    std::vector<CItem*> items;
    for (int i = 0; i < 11; ++i) {
        CItem* it = new CItem();
        it->set_name("物件", "thing");
        it->move(u);
        items.push_back(it);
    }
    items[3]->set("equipped", (long)2);  // move 后再置标志（move 会清装备）
    u->clear();  // 隔离 move 期间 on_add_object 的 ADDOBJECT 包

    u->flush_my_equip();
    ASSERT_EQ(u->sent, 2);

    std::string head0  = "&C=" + std::to_string(static_cast<int>(RTNITEM))
                         + "&B=0&T=" + std::to_string(u->object_id()) + "&";
    std::string head10 = "&C=" + std::to_string(static_cast<int>(RTNITEM))
                         + "&B=10&T=" + std::to_string(u->object_id()) + "&";
    EXPECT_EQ(u->msgs[0].compare(0, head0.size(), head0), 0) << u->msgs[0];
    EXPECT_EQ(u->msgs[1].compare(0, head10.size(), head10), 0) << u->msgs[1];

    // 索引连续 0..10、条目无丢失、落包正确
    int idx = 0;
    for (auto* item : *u->get_item_list()) {
        std::string entry      = "I" + std::to_string(idx) + "="
                                 + std::to_string(item->object_id()) + "&";
        const std::string& pkg = (idx < 10) ? u->msgs[0] : u->msgs[1];
        EXPECT_NE(pkg.find(entry), std::string::npos) << "缺失条目 " << entry;
        ++idx;
    }
    EXPECT_EQ(idx, 11);

    // equipped 标志与同索引 I 条目紧邻（E<i>=2）
    int eq_idx = 0;
    for (auto* item : *u->get_item_list()) {
        if (item == items[3]) break;
        ++eq_idx;
    }
    std::string eq_entry = "I" + std::to_string(eq_idx) + "="
                           + std::to_string(items[3]->object_id()) + "&E"
                           + std::to_string(eq_idx) + "=2&";
    EXPECT_NE(u->msgs[(eq_idx < 10) ? 0 : 1].find(eq_entry), std::string::npos)
        << "缺失装备标志 " << eq_entry;

    for (auto* it : items) delete it;
}

// ---------------- flush_my_skill ----------------
// 包发给收件人 me：T= 是 subject 的 object_id，P= 仅 this==me，
// B=1/E=1 框架位固定首/末包（技能身份无序 → 按内容集合断言）。
TEST_F(CharFlushTest, FlushMySkill_RecipientAndFrames)
{
    StateRecordingUser* viewer = new StateRecordingUser();

    // 场景A other：subject=u，收件人=viewer（this != me）
    u->flush_my_skill(viewer);
    ASSERT_EQ(viewer->sent, 3);  // u 恰有 3 技能
    EXPECT_EQ(u->sent, 0) << "投递不应落回 subject 自己";
    std::string subject_tid = "&T=" + std::to_string(u->object_id()) + "&";
    for (auto& m : viewer->msgs) {
        EXPECT_NE(m.find(subject_tid), std::string::npos) << m;
        EXPECT_EQ(m.find("&P="), std::string::npos)
            << "this != me 不应有升级点数 " << m;
    }
    // 框架位：首包 B=1、中包无、末包 E=1
    EXPECT_NE(viewer->msgs[0].find("&B=1&"), std::string::npos);
    EXPECT_EQ(viewer->msgs[0].find("&E=1&"), std::string::npos);
    EXPECT_EQ(viewer->msgs[1].find("&B=1&"), std::string::npos);
    EXPECT_EQ(viewer->msgs[1].find("&E=1&"), std::string::npos);
    EXPECT_NE(viewer->msgs[2].find("&E=1&"), std::string::npos);
    EXPECT_EQ(viewer->msgs[2].find("&B=1&"), std::string::npos);
    // 每技能一包、等级正确（无序 → 集合断言）
    for (auto* skl : {"unarmed", "dodge", "force"}) {
        int hits = 0;
        for (auto& m : viewer->msgs) {
            if (m.find(std::string("&S=") + skl + "&L=") != std::string::npos)
                ++hits;
        }
        EXPECT_EQ(hits, 1) << "技能包缺失或重复: " << skl;
    }
    delete viewer;

    // 场景B self：this == me → 每包带 P=，并带 M=/K= 标记
    u->clear();
    u->query_skill_map()->set("unarmed", "pingfeng");
    u->query_skill_prepare()->set("dodge", "虚招");
    u->flush_my_skill(u);
    ASSERT_EQ(u->sent, 3);
    // P= 值 oracle：skill_need_points / learn_bonus + 1
    int32_t pt        = u->learn_bonus();
    int32_t p_unarmed = u->skill_need_points("unarmed") / pt + 1;
    bool has_p        = false;
    bool has_m        = false;
    bool has_k        = false;
    for (auto& m : u->msgs) {
        if (m.find("&P=" + std::to_string(p_unarmed) + "&")
            != std::string::npos)
            has_p = true;
        if (m.find("&M=pingfeng&") != std::string::npos) has_m = true;
        if (m.find("&K=1&") != std::string::npos) has_k = true;
    }
    EXPECT_TRUE(has_p) << "self 视图应携带升级点数 P=" << p_unarmed;
    EXPECT_TRUE(has_m) << "映射技能应携带 M= 标记";
    EXPECT_TRUE(has_k) << "装备技能应携带 K=1 标记";
}

// ---------------- send_table ----------------
TEST_F(CharFlushTest, SendTable_Empty_NoPacket)
{
    CMapping table;
    u->send_table("empty", &table, 0, 1);
    EXPECT_EQ(u->sent, 0);
}

// 单行 → st=3（首末合一）+ 表头，整包逐字节断言（单键顺序确定）
TEST_F(CharFlushTest, SendTable_SingleRow_St3)
{
    CMapping table;
    CMapping row;
    row.set("name", "唯一");
    table.set("k1", row);
    u->send_table("one", &table, 1, 1);
    ASSERT_EQ(u->sent, 1);
    std::string expect = "&C=" + std::to_string(static_cast<int>(SENDTABLE))
                         + "&st=3&T=one&R=1&P=1&name=唯一&";
    EXPECT_EQ(u->msgs[0], expect);
}

// 3 行 → 恰一包 st=1（含表头 T/R/P）、恰一包 st=2、恰一包无 st 标记；
// 行序无序 → 按 st 框架与行值集合断言。
TEST_F(CharFlushTest, SendTable_MultiRows_Frame)
{
    CMapping table;
    const char* values[] = {"物品A", "物品B", "物品C"};
    const char* keys[]   = {"k1", "k2", "k3"};
    for (int i = 0; i < 3; ++i) {
        CMapping row;
        row.set("name", values[i]);
        table.set(keys[i], row);
    }
    u->send_table("multi", &table, 7, 2);
    ASSERT_EQ(u->sent, 3);

    int st_first = 0, st_last = 0, st_none = 0;
    for (auto& m : u->msgs) {
        std::string st = field(m, "st");
        if (st == "1") {
            ++st_first;
            // 表头仅随首行
            EXPECT_EQ(field(m, "T"), "multi") << m;
            EXPECT_EQ(field(m, "R"), "7") << m;
            EXPECT_EQ(field(m, "P"), "2") << m;
        }
        else if (st == "2") {
            ++st_last;
            EXPECT_EQ(field(m, "T"), "") << "末行不应重复表头 " << m;
        }
        else if (st.empty()) {
            ++st_none;  // 中间行无 st 标记
            EXPECT_EQ(field(m, "T"), "") << "中间行不应带表头 " << m;
        }
        else {
            ADD_FAILURE() << "意外的 st 标记: " << st << " in " << m;
        }
    }
    EXPECT_EQ(st_first, 1);
    EXPECT_EQ(st_last, 1);
    EXPECT_EQ(st_none, 1);

    // 行内容不丢（行序无序 → 按值断言出现恰好一次）
    for (auto* v : values) {
        int hits = 0;
        for (auto& m : u->msgs) {
            if (m.find(std::string("name=") + v + "&") != std::string::npos)
                ++hits;
        }
        EXPECT_EQ(hits, 1) << "行值缺失或重复: " << v;
    }
}

// ---------------- 真实传输层（CUser::send_command） ----------------
// 本文件是该路径的唯一测试 owner：
//  1) buffered 追加（wait=1 或缓冲非空合并）、
//  2) wait=0 且缓冲空 → 立即 send_off → send_to_client(serial=0 判空)、
//  3) 无客户端 → has_client 早退，缓冲不动。
TEST_F(CharFlushTest, Transport_BufferedAndImmediate)
{
    CMapping table;
    CMapping row;
    row.set("name", "x");
    table.set("k", row);

    // 1) buffered：flush 进缓冲，不立即投递
    TransportProbeUser probe;
    probe.set_temp("client/serial", 0);
    probe.flush_my_equip();
    EXPECT_EQ(probe.send_off_count(), 0);
    EXPECT_GT(probe.buffered_len(), 0);
    int len_after_flush = probe.buffered_len();

    // 2) wait=0 但缓冲非空 → 合并追加，仍不投递
    probe.send_table("t", &table, 1, 1);
    EXPECT_EQ(probe.send_off_count(), 0);
    EXPECT_GT(probe.buffered_len(), len_after_flush);

    // 3) wait=0 且缓冲空 → 立即投递：send_off 恰一次、缓冲归零。
    //    send_to_client(serial=0) → (CGateInfo*)null → 安全空操作。
    TransportProbeUser fresh;
    fresh.set_temp("client/serial", 0);
    fresh.send_table("t", &table, 1, 1);
    EXPECT_EQ(fresh.send_off_count(), 1);
    EXPECT_EQ(fresh.buffered_len(), 0);

    // 4) 无客户端 → send_command 早退，什么都不做
    TransportProbeUser naked;  // 未设 client/serial
    naked.flush_my_equip();
    naked.send_table("t", &table, 1, 1);
    EXPECT_EQ(naked.send_off_count(), 0);
    EXPECT_EQ(naked.buffered_len(), 0);
}

// ---------------- 窃听分支（User.cpp send_command 62-97） ----------------
namespace {
// 窃听探针：每次投递计数；可复位发送缓冲，让每条消息都走
// wait=0 立即投递路径，从而对每个窃听分支做逐条可观测断言。
class SnoopProbeUser : public CUser {
public:
    void send_off() override
    {
        ++m_send_off_count;
        CUser::send_off();
    }
    int count() const { return m_send_off_count; }
    void reset_buffer() { m_buffer_len = 0; }

private:
    int m_send_off_count = 0;
};
}  // namespace

TEST_F(CharFlushTest, SendCommandSnoopBranches)
{
    SnoopProbeUser snoop;
    snoop.set_name("窃听者", "snoop1");
    snoop.set_temp("client/serial", 0);  // client 子表非空 → has_client
    snoop.set_temp("client/snoop", 1);   // 打开窃听态（truthy）
    // 未设 block_msg/all → NOTIFY/COMMON_ATTACK 不在首守卫提前 return

    char m[256];

    // 62: NOTIFY / COMMON_ATTACK → do_snooping(M)
    snprintf(m, sizeof(m), "&C=%d&M=%s&", NOTIFY, "chat");
    snoop.send_command(m, 0);
    EXPECT_EQ(1, snoop.count()) << "NOTIFY 窃听后仍完成投递(62-63)";
    snoop.reset_buffer();
    snprintf(m, sizeof(m), "&C=%d&M=%s&", COMMON_ATTACK, "hit");
    snoop.send_command(m, 0);
    EXPECT_EQ(2, snoop.count()) << "COMMON_ATTACK 同支(62)";
    snoop.reset_buffer();

    // 65: TELLOBJECT → 私聊格式 do_snooping(n/M)
    snprintf(m, sizeof(m), "&C=%d&n=%s&M=%s&", TELLOBJECT, "tom", "hi");
    snoop.send_command(m, 0);
    EXPECT_EQ(3, snoop.count()) << "TELLOBJECT 窃听(65-70)";
    snoop.reset_buffer();

    // 71: GOTOROOM → 房间名格式 do_snooping(N)
    snprintf(m, sizeof(m), "&C=%d&N=%s&", GOTOROOM, "pk");
    snoop.send_command(m, 0);
    EXPECT_EQ(4, snoop.count()) << "GOTOROOM 窃听(71-75)";
    snoop.reset_buffer();

    // 76: QUESTION 带 s → "()s" 分支(79)
    snprintf(m, sizeof(m), "&C=%d&s=%s&", QUESTION, "who");
    snoop.send_command(m, 0);
    EXPECT_EQ(5, snoop.count()) << "QUESTION(s) 窃听(76-79)";
    snoop.reset_buffer();

    // 76: QUESTION 无 s 有 m → "(n)m" 分支(82-84)
    snprintf(m, sizeof(m), "&C=%d&m=%s&n=%s&", QUESTION, "what", "bob");
    snoop.send_command(m, 0);
    EXPECT_EQ(6, snoop.count()) << "QUESTION(m) 窃听(82-84)";
    snoop.reset_buffer();

    // 90: NOTIFY_FAIL → "(F)M" 分支(91-94)
    snprintf(m, sizeof(m), "&C=%d&F=%s&M=%s&", NOTIFY_FAIL, "cmd", "err");
    snoop.send_command(m, 0);
    EXPECT_EQ(7, snoop.count()) << "NOTIFY_FAIL 窃听(90-94)";
    snoop.reset_buffer();

    // 关闭窃听 → 跳过分支但无条件走到投递(97)
    snoop.delete_temp("client/snoop");
    snprintf(m, sizeof(m), "&C=%d&M=%s&", NOTIFY, "plain");
    snoop.send_command(m, 0);
    EXPECT_EQ(8, snoop.count()) << "无窃听直接投递(97)";
}
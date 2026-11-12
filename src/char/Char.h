/**
 * @file Char.h
 * @brief 角色基类，管理属性/经验/技能/战斗/状态。
 */

#pragma once

#include "Container.h"
#include "IHittable.h"
#include "IEquippable.h"
#include "IActionHandler.h"
#include "IDialogSource.h"
#include "IClientObject.h"

extern int cc_strcmp(const char* str1, const char* str2);
inline int cc_strcmp(const std::string& str1, const char* str2)
{
    return cc_strcmp(str1.c_str(), str2);
}
inline int cc_strcmp(const char* str1, const std::string& str2)
{
    return cc_strcmp(str1, str2.c_str());
}
inline int cc_strcmp(const std::string& str1, const std::string& str2)
{
    return cc_strcmp(str1.c_str(), str2.c_str());
}
#define DIFFERSTR(x, y) (cc_strcmp(x, y) != 0)
#define EQUALSTR(x, y)  (cc_strcmp(x, y) == 0)
#define RANDOM(x, y)    g_utils.random(x)

#define CND_CONTINUE   1
#define CND_NO_HEAL_UP 2
#define IDLE_TIMEOUT   900

typedef int BUSYFUNCTION(CChar* ob);
typedef int INTERRUPTFUNCTION(CChar* me, CChar* who, char* how);

/**
 * @brief 角色基类，玩家和 NPC 的共同父类，管理经验/技能/战斗/状态。
 *
 * 实现接口：IHittable, IEquippable, IActionHandler, IDialogSource,
 * IClientObject。
 */
class CChar : public CContainer,
              public IHittable,
              public IEquippable,
              public IActionHandler,
              public IDialogSource,
              public IClientObject {
public:
    enum Action {
        DO_WIELD,
        DO_UNWIELD,
        DO_USE,
        DO_LIST,
        DO_TALK,
    };

    // === IHittable ===
    const char* hit_ob(CChar* me, CChar* victim,
                       damage_t damage_bonus) override;
    void owner_is_killed(CChar* killer) override;

    // === IEquippable ===
    int wear() override;
    int wield() override;

    // === IActionHandler ===
    int do_action(int act, CChar* me = NULL) override;
    int handle_action(char* comm, CChar* me, char* arg) override;
    int handle_action(char* comm, CChar* me, char* arg, char* tar) override;

    // === IDialogSource ===
    void send_menu(CChar* me) override;
    void add_menu_item(string show, string cmd, CChar* me) override;
    int say(const char* msg, CChar* me = NULL, const char* tips = 0) override;

    // === IClientObject ===
    void download(CChar* me) override;
    void get_info(char* str, CChar* me) override;
    int do_look(CChar* me) override;
    void send_command(const char* msg, int wait = SEND_WAIT_NONE) override;

    static const char* eff_status_msg(ratio_t ratio);

    virtual void attack(CChar* opponent);

    void send_table(const char* table_name, CMapping* table, int total,
                    int page);

    void reset_level();

    int follow_me(CChar* me, char* dir);
    CChar* query_leader();
    void set_leader(CChar* me);
    virtual void is_defeated(CChar* winnder, CChar* failer);
    CMapping* query_action();

    exp_t query_magic_exp();

    int query_magic_skill(const char* skill, int raw = 0);

    void add_balance(money_t val);

    void remove_all_killer();
    void set_killer(CChar* ob);
    exp_t query_valid_exp();
    count_t skill_need_points(const char* skill);
    virtual int recognize_apprentice(CChar* me);

    // 等级/经验换算：入参一律收槽位宽（调用方读出来的就是槽位值），
    // 出参按数据域——等级是 bounded 的 level_t，经验是 exp_t。
    static level_t exp2level(exp_t exp);
    static skill_level_t level2skill(mapping_value_t lv);
    static skill_level_t skill2level(mapping_value_t sk);
    static void reset_level_exp_cache();  // 重置 level_exp 缓存（供测试使用）

    // 将状态标志（战斗/中毒/发呆/disabled/忙碌 等）追加到 msg。
    void get_state_msg(std::string& msg);
    // 发送角色状态包。broadcast=true 时额外广播给房间内其他玩家；
    // 非玩家对象仅在广播时发送精简包，不计算/私发完整状态。
    void send_state(bool broadcast = true);
    virtual void init(CChar* me);

    void fight_ob(CChar* ob);
    void kill_ob(CChar* ob);
    int is_killing(CChar* ob);

    mapping_value_t query_condition(const char* cnd);

    int is_fighting(CContainer* enemy = NULL);
    int is_busy();

    void improve_skill(const char* skill, count_t amount, flag_t weak_mode = 0);
    void improve_skill(const std::string& skill, count_t amount,
                       flag_t weak_mode = 0)
    {
        return improve_skill(skill.c_str(), amount, weak_mode);
    }
    int skill_death_recover();
    int skill_death_penalty();

    CMapping* query_learned();
    CMapping* query_skills();
    CMapping* query_skill_prepare();
    CMapping* query_skill_map();
    void clear_skill_prepare();
    void clear_skill_map();
    void clear_skill();
    skill_level_t query_skill(const char* skill, int raw = 0);
    skill_level_t query_skill(const std::string& skill, int raw = 0)
    {
        return query_skill(skill.c_str(), raw);
    }
    const char* query_skill_prepared(const char* skill);
    const char* query_skill_mapped(const char* skill);
    const char* query_skill_mapped(const std::string& skill)
    {
        return query_skill_mapped(skill.c_str());
    }
    void prepare_skill(const char* skill, const char* mapped_to = NULL);
    void map_skill(const char* skill, const char* mapped_to = NULL);
    int delete_skill(const char* skill);
    int set_skill(const char* skill, int level);

    // 施加持续效果：脚本缺失/损坏时拒绝写入并返回 false
    bool apply_condition(const char* cnd, int32_t duration, int damage = 0);
    bool apply_condition(CChar* who, const char* cnd, int32_t duration,
                         int damage = 0);
    int update_condition();

    virtual int prevent_learn(CChar* me, const char* skill);
    void set_title(const char* title);

    typedef map<int32_t, int32_t, less<int32_t> > DTEquipList;
    typedef DTEquipList::iterator iterator_equip;

    static exp_t level2_exp(mapping_value_t lv);
    virtual void send_off();

    void assign_apprentice(const char* title, int privs);
    void create_family(char* family, int generation, char* title);
    virtual void attempt_apprentice(CChar* me);
    int recruit_apprentice(CChar* me);
    int is_apprentice_of(CChar* me);

    CContainer* query_weapon();
    CContainer* query_left_weapon();
    // 数值属性统查，name 取 str/int/con/per/dex/combat_exp（可取键只此一
    // 处）。前五项含 apply/ 装备修正与对应技能加成，combat_exp 为原始历练
    // 值。未知名是调用方写错键：传 ok 时交回调用方处置，否则记错误日志。
    mapping_value_t attr(const char* name, bool* ok = nullptr);

    void unconcious();
    virtual void die();
    int command(char* arg);

    virtual int save();
    virtual int load();

    void start_busy(count_t start, INTERRUPTFUNCTION* new_interrupt = NULL,
                    CChar* src = NULL);
    void interrupt_me(CChar* who, char* how);
    void start_busy(BUSYFUNCTION* new_busy,
                    INTERRUPTFUNCTION* new_interrupt = NULL);

    damage_t receive_healing(const char* type, damage_t heal);
    damage_t receive_curing(const char* type, damage_t heal);
    damage_t receive_damage(const char* type, damage_t damage,
                            CChar* who = NULL);
    damage_t receive_wound(const char* type, damage_t damage, CChar* me = NULL);
    damage_t receive_poison(const char* type, damage_t damage,
                            const char* condi);

    int disable_player(const char* szType);
    int enable_player();

    virtual void setup();
    void heart_beat();
    int32_t query_equip(int pos);
    DTEquipList* get_equip_list();
    virtual int32_t query_attr(int attr);
    CChar();
    virtual ~CChar();

public:
    team_index_t query_team();
    void revive(int quiet);

    int load_from_server();
    int save_to_server();

    int cmd_hit_ob(CChar* target, char* arg);
    int can_learn(const char* skillname);
    void flush_my_skill(CChar* me);
    void flush_my_equip();
    void flush_my_info();
    static void remove_betrayer(CChar* me);
    int query_unarmed();
    virtual void finish_kill(CChar* me);
    int learn_bonus();
    flag_t refuse_channel(const char* channel);

    flag_t is_loading();
    wizard_level_t query_grant();
    count_t recruit_num();
    void release_master();
    void remove_killer(CChar* ob);
    virtual void relay_say(CChar* me, char* msg);
    virtual void relay_emote(CChar* me, string verb);
    static void remove_faint(CContainer* me, int32_t param1, int32_t param2);
    int get_save_data(CMapping* save);
    int restore_data(CMapping* save);

protected:
    const char* inventory_look(CContainer* item, int flag);

    int heal_up();

    void continue_action();

    // 装备位置
    DTEquipList m_equip_list;  // 装备位置

    count_t m_tick;
    count_t m_tick_cnd;
    count_t m_tick_two;
    int m_wait_loading;  // 切换地图等待

    count_t m_busy_count;
    BUSYFUNCTION* m_busy_func;
    INTERRUPTFUNCTION* m_interrupt;

    count_t m_revive;  // 苏醒秒数

    CMapping skills;
    CMapping learned;
    CMapping skill_map;
    CMapping skill_prepare;

    CVector m_killers;  // 敌人

    CChar* m_leader;

    count_t m_save_count;

    int m_begin_menu = 1;  // 菜单状态（原 CContainer 成员）
};

void setup_char(CChar* me);
void setup_human(CChar* me);
void setup_human_base_attrs(CChar* me);
void setup_human_max_hp(CChar* me, mapping_value_t age);
void setup_human_npc(CChar* me);
void drop_all(CChar* me);

const char* query_waimao(string gender, mapping_value_t per);
const char* query_rank(CChar* ob);

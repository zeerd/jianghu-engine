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
    const char* hit_ob(CChar* me, CChar* victim, int32_t damage_bonus) override;
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

    static const char* eff_status_msg(int ratio);

    virtual void attack(CChar* opponent);

    void send_table(const char* table_name, CMapping* table, int total,
                    int page);

    void reset_level();

    int follow_me(CChar* me, char* dir);
    CChar* query_leader();
    void set_leader(CChar* me);
    virtual void is_defeated(CChar* winnder, CChar* failer);
    CMapping* query_action();

    unsigned long query_magic_exp();

    int query_magic_skill(const char* skill, int raw = 0);

    void add_balance(int32_t val);

    int32_t query_combat_exp();
    int32_t query_exp();
    void remove_all_killer();
    void set_killer(CChar* ob);
    int32_t query_valid_exp();
    int32_t skill_need_points(const char* skill);
    virtual int recognize_apprentice(CChar* me);

    static int exp2level(int32_t exp);
    static int level2skill(int lv);
    static int skill2level(int sk);
    static void reset_level_exp_cache();  // 重置 level_exp 缓存（供测试使用）

    // 将状态标志（战斗/中毒/发呆/disabled 等）追加到 msg。
    void get_state_msg(std::string& msg);
    // 发送角色状态包。broadcast=true 时额外广播给房间内其他玩家；
    // 非玩家对象仅在广播时发送精简包，不计算/私发完整状态。
    void send_state(bool broadcast = true);
    virtual void init(CChar* me);

    void fight_ob(CChar* ob);
    void kill_ob(CChar* ob);
    int is_killing(CChar* ob);

    int32_t query_condition(const char* cnd);
    void clear_condition();

    int is_fighting(CContainer* enemy = NULL);
    int is_busy();

    void improve_skill(const char* skill, int amount, int weak_mode = 0);
    void improve_skill(const std::string& skill, int amount, int weak_mode = 0)
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
    int query_skill(const char* skill, int raw = 0);
    int query_skill(const std::string& skill, int raw = 0)
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

    static int32_t level2_exp(int lv);
    virtual void send_off();

    void assign_apprentice(const char* title, int privs);
    void create_family(char* family, int generation, char* title);
    virtual void attempt_apprentice(CChar* me);
    int recruit_apprentice(CChar* me);
    int is_apprentice_of(CChar* me);

    CContainer* query_weapon();
    CContainer* query_left_weapon();
    int32_t query_str();
    int32_t query_con();
    int32_t query_int();
    int32_t query_dex();

    void unconcious();
    virtual void die();
    int command(char* arg);

    virtual int save();
    virtual int load();

    void start_busy(int start, INTERRUPTFUNCTION* new_interrupt = NULL,
                    CChar* src = NULL);
    void interrupt_me(CChar* who, char* how);
    void start_busy(BUSYFUNCTION* new_busy,
                    INTERRUPTFUNCTION* new_interrupt = NULL);

    int32_t receive_healing(const char* type, int32_t heal);
    int32_t receive_curing(const char* type, int32_t heal);
    int32_t receive_damage(const char* type, int32_t damage, CChar* who = NULL);
    int32_t receive_wound(const char* type, int32_t damage, CChar* me = NULL);
    int32_t receive_poison(const char* type, int32_t damage, const char* condi);

    int disable_player(const char* szType);
    int enable_player();

    virtual void setup();
    void heart_beat();
    static char* wield_position(int pos);
    int32_t query_equip(int pos);
    DTEquipList* get_equip_list();
    virtual int32_t query_attr(int attr);
    CChar();
    virtual ~CChar();

public:
    int query_team();
    void revive(int quiet);

    int load_from_server();
    int save_to_server();

    int cmd_hit_ob(CChar* target, char* arg);
    int can_learn(const char* skillname);
    void flush_my_skill(CChar* me);
    void flush_my_equip();
    void flush_my_info();
    int mini_skill();
    static void remove_betrayer(CChar* me);
    int query_unarmed();
    virtual void finish_kill(CChar* me);
    int learn_bonus();
    int refuse_channel(const char* channel);
    int32_t query_apply(const char* attr);

    int is_invalid();

    int is_loading();
    CContainer* query_shield();
    int query_grant();
    int recruit_num();
    void release_master();
    void remove_killer(CChar* ob);
    virtual void relay_say(CChar* me, char* msg);
    virtual void relay_emote(CChar* me, string verb);
    int query_per();
    static void remove_faint(CContainer* me, int32_t param1, int32_t param2);
    int get_save_data(CMapping* save);
    int restore_data(CMapping* save);

protected:
    const char* inventory_look(CContainer* item, int flag);

    int heal_up();

    void continue_action();

    // 装备位置
    DTEquipList m_equip_list;  // 装备位置

    int m_tick;
    int m_tick_cnd;
    int m_tick_two;
    int m_wait_loading;  // 切换地图等待

    int m_busy_count;
    BUSYFUNCTION* m_busy_func;
    INTERRUPTFUNCTION* m_interrupt;

    int m_revive;  // 苏醒秒数

    CMapping skills;
    CMapping learned;
    CMapping skill_map;
    CMapping skill_prepare;

    CVector m_killers;  // 敌人

    CChar* m_leader;

    int m_save_count;

    int m_begin_menu = 1;  // 菜单状态（原 CContainer 成员）
};

void setup_char(CChar* me);
void setup_human(CChar* me);
void setup_human_base_attrs(CChar* me);
void setup_human_max_hp(CChar* me, int age);
void setup_human_npc(CChar* me);
void drop_all(CChar* me);

const char* query_waimao(string gender, int per);
const char* query_rank(CChar* ob);

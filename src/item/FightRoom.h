/**
 * @file FightRoom.h
 * @brief 战斗精灵房间，管理组队/进出战斗/回合控制。
 */

#pragma once

#include <memory>
#include <vector>
#include "Item.h"

#define MAXFIGHTERS    16
#define FIGHT_NO_FLEE  1
#define FIGHT_NO_GUARD 2
#define FIGHT_LOCKED   3

typedef std::vector<CChar*> TeamList;

/**
 * @brief 战斗精灵房间，管理多队战斗的回合制逻辑。
 */
class CFightRoom : public CItem {
public:
    //---------- 队伍标识 ----------
    enum {
        FIGHT_TEAM_A = 0,
        FIGHT_TEAM_B,
    };

    //---------- 构造 / 析构 ----------
    CFightRoom();
    virtual ~CFightRoom();

    //---------- 战斗流程 ----------
    virtual void create(int flag = 0);
    void begin_fight(int flag = 0, int hp_check = 50);
    void end_fight();
    virtual void heart_beat();
    int do_flee(CChar* me);
    bool is_threshold_match();

    //---------- 队伍管理 ----------
    int join_team(CChar* ob, int Team);  // Join_Team(me, FIGHT_TEAM_A);
    void leave_team(CChar* obj);
    void do_leave_team(CChar* obj);
    int can_join(int team, CChar* me = NULL);
    CChar* select_member(int team);
    int get_team_count(int team);
    std::unique_ptr<TeamList> get_team_list(int team, CChar* target = NULL,
                                            int num = 1);  // 战队成员选择

    //---------- 查询 / 协议输出 ----------
    virtual int32_t query_attr(int attr);
    virtual void get_info(char* str, CChar* me);
    char* get_team_msg(char* msg, int team);
    virtual void on_leave(CContainer* me);

    //---------- 对外可观测状态 ----------
    int turn;       // 当前回合号：奇数 team0 行动，偶数 team1 行动
    int tick;       // 心跳分频计数：每 2 次心跳推进一个回合
    bool m_single;  // 单人作战（Do_BiShi 与 Lua 绑定直接读写）

    // 两队名册：战斗逻辑与测试直接读写
    struct TeamSlot {
        CChar* obj[MAXFIGHTERS];
        int32_t num;          // 取槽游标
        int32_t fighter_num;  // 在队人数
    } m_Team[2];

protected:
    //---------- 内部辅助 ----------
    int32_t query_turn_ap(CChar* me);
    void clear_team_slot(int team, int idx, CChar* obj);
    void advance_ap();
    bool run_team_turn(int myTeam);

    //---------- 战斗规则配置 ----------
    int m_fighters_max;      // 一队最多的参战人数
    bool m_threshold_match;  // 阈值对局：不许中途加入、不加血、血量到阈值即判负
    int m_hp_check;          // 判负血量百分比阈值

    //---------- 回合内部控制量 ----------
    int m_tick;  // 连续找不到攻击目标的心跳计数，满 3 次收场
};

/**
 * @file Global.h
 * @brief 全局头文件，汇总各子系统头文件与全局常量定义。
 */

#pragma once

#include "MainConfig.h"

// --- SendCommand wait 参数 ---
#define SEND_WAIT_NONE      0  // 默认（无缓冲时立即发）
#define SEND_WAIT_BUFFERED  1  // 缓冲后批量发送
#define SEND_WAIT_IMMEDIATE 2  // 立即强制发送

// --- 登录协议字段 ---
#define PROTO_FIELD_ERROR  1  // F=1 错误标记
#define PROTO_FIELD_BULLET 1  // B=1 公告标记

#include <cstdint>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <map>
#include <list>
#include <mutex>
#include "Logger.h"

// --- 新拆分模块头文件 ---
#include "../engine/GameInit.h"
#include "../engine/HeartBeat.h"
#include "../item/ItemMgr.h"
#include "../npc/NpcMgr.h"
#include "../room/RoomMgr.h"
#include "../user/UserMgr.h"
#include "../skill/SkillMgr.h"
#include "../net/GateClient.h"
#include "../util/StringUtil.h"
#include "../engine/GameObject.h"

using namespace std;

class CCmdHandle;
class CContainer;
class CItem;
class CNpc;
class CSkill;
class CCondition;
class CRoom;
class CUser;
class CChar;
class CChannel;
class CCombatd;
class CReward;
class CAutoFight;
class CWeaponEffect;
class CCombatCalc;
class CCombatAnnounce;
class CCombatConfig;
class CFightRoom;
class CVendor;

// 全局

// 客户端接受指令
enum {
    GOTOROOM = 10,
    ADDOBJECT,
    REMOVEOBJECT,
    RTNSKILL,
    RTNITEM,
    RTNINFO,
    DOWNSKILL,      // 下载技能
    DOWNOBJECT,     // 下载物件
    LOGINUSER,      // 登录
    NOTIFY,         // 通知消息
    WAKEUP,         // 唤醒
    DOWNEMOTE,      // 下载表情
    COMMON_ATTACK,  // 普通攻击
    // RTNSTATUS,			//战斗状态
    STATE,        // 指定状态
    ADDDOOR,      // 增加出口
    SENDUSER,     // 发布用户信息
    VENDOR_LIST,  // 商人货品清单
    QUESTION,     // 问题
    DOWNMAGIC,    // 下法术
    NOTIFY_FAIL,  // 指令执行失败通知
    BULLET,       // 公告版
    TELLOBJECT,   // 私聊
    LOCATION,     // 定位
    USERLIST,     // 用户列表
    DOWNUSER,     // 下载用户资料
    MAGICCAST,    // 魔法效果
    SYN_AP,       // 同步ＡＰ
    SENDTABLE,    // 发送表信息
    CONTROLNPC,   // 控制NPC标记
    ADDLOG,       // 增加日志
};

// Equip Position
// 左右手区别角色的装备位置。手指武器可装备位置，可佩带在左右手上。
enum {
    WIELD_NONE = 0,
    WIELD_HEAD,         // 头	头盔
    WIELD_LEFT_ERR,     // 左耳
    WIELD_RIGHT_ERR,    // 右耳
    WIELD_ERR,          // 耳	魔法物
    WIELD_NECK,         // 颈	项圈
    WIELD_LEFT_HAND,    // 左手	盾牌，箭袋
    WIELD_RIGHT_HAND,   // 右手	通常用来装备武器
    WIELD_HAND,         // 手
    WIELD_LEFT_WRIST,   // 左腕	手环可以戴在左右腕上。
    WIELD_RIGHT_WRIST,  // 右腕
    WIELD_WRIST,        // 腕
    WIELD_ARMOR,        // 铠甲
    WIELD_WAIST,        // 腰带
    WIELD_BOOT,         // 靴子
    WIELD_MANTLE,       // 斗篷
    WIELD_ARMOR_HANDS,  // 手套
    WIELD_BOTH_HAND,    // 双手
    WIELD_RING,         // 指环
    WIELD_CLOTH,        // 衣服
};

typedef map<string, CSkill*, less<string> > MAP2SKILL;
typedef MAP2SKILL::iterator iterator_skill;
typedef map<string, CCondition*, less<string> > MAP2CND;
typedef MAP2CND::iterator iterator_condition;
typedef map<string, CRoom*, less<string> > MAP2ROOM;
typedef MAP2ROOM::iterator iterator_room;
typedef map<int32_t, CContainer*, less<int32_t> > MAP2ITEM;
typedef MAP2ITEM::iterator iterator_item;
typedef map<int32_t, CNpc*, less<int32_t> > MAP2NPC;
typedef MAP2NPC::iterator iterator_npc;
typedef map<int32_t, CUser*, less<int32_t> > MAP2USER;
typedef MAP2USER::iterator iterator_user;
typedef map<int32_t, CVendor*, less<int32_t> > MAP2VENDOR;
typedef MAP2VENDOR::iterator iterator_vendor;

extern MAP2USER g_user_list;               // User List
extern MAP2ITEM g_item_list;               // Item List
extern MAP2NPC g_npc_list;                 // NPC List
extern MAP2ROOM g_room_list;               // Room List
extern MAP2SKILL g_skill_list;             // Skill List
extern MAP2CND g_condition_list;           // Condition List
extern MAP2VENDOR g_vendor_list;           // Vendor List
extern CChannel g_channel;                 // Channel Object
extern CCombatd g_combatd;                 // Combat Object
extern CReward g_reward;                   // Reward Object
extern CAutoFight g_auto_fight;            // NPC Auto-Fight AI
extern CCombatAnnounce g_combat_announce;  // Combat Announcements
extern CCombatConfig g_combat_config;      // Combat System Config
extern const char* g_color[13];
extern CCmdHandle g_command_handler;

extern int32_t g_last_destruct_object;  // 上次被销毁的物件。

MAP2USER* users();
int notify_fail(const char* fmt, ...);

// --- 新拆分函数声明头文件 ---
#include "../net/Message.h"
#include "../engine/Rank.h"
#include "../net/Login.h"
#include "../util/Random.h"

#define DESTRUCT_OB(ob, func) destruct_with_logs(ob, func)

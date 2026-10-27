/**
 * @file Global.cpp
 * @brief 全局变量定义 */

#include "Global.h"
#include "Logger.h"
#include "CmdHandle.h"
#include "Channel.h"
#include "Combatd.h"
#include "CReward.h"
#include "AutoFight.h"
#include "CombatAnnounce.h"
#include "CombatConfig.h"

#include "LuaBindEnums.h"

//---------------------------------------------------------
// 全局对象实例
//---------------------------------------------------------
CCmdHandle g_command_handler;
CChannel g_channel;
CCombatd g_combatd;
CReward g_reward;
const char* g_color[] = {"$HIY", "$HIG", "$RED", "$MAG", "$YEL", "$HIC", "$HIW",
                         "$HIR", "$HIB", "$CYN", "$HIW", "$HIM", "$BLU"};

FILE* g_debug_file;
bool g_reboot = false;

MAP2USER g_user_list;
MAP2ITEM g_item_list;
MAP2NPC g_npc_list;
MAP2ROOM g_room_list;
MAP2SKILL g_skill_list;
MAP2CND g_condition_list;
MAP2VENDOR g_vendor_list;

int g_current_account = 0;
int32_t g_last_destruct_object;

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// current_time_info() -> table{wday, hour} -
// 返回服务器当前星期几(0=周日)和小时(0-23)
int l_current_time_info(lua_State* L)
{
    time_t t           = time(NULL);
    struct tm* newtime = localtime(&t);
    lua_newtable(L);
    lua_pushinteger(L, newtime->tm_wday);
    lua_setfield(L, -2, "wday");
    lua_pushinteger(L, newtime->tm_hour);
    lua_setfield(L, -2, "hour");
    return 1;
}

// get_server_id() -> number - 当前服务器编号（对应全局 sys_server_id）
// 十八地狱怪物掉宝表（case 7）按服务器编号区分书本池大小
int l_get_server_id(lua_State* L)
{
    lua_pushinteger(L, g_server_id);
    return 1;
}

// current_time() -> number - 获取游戏当前时间（心跳 tick）
int l_current_time(lua_State* L)
{
    lua_pushinteger(L, g_current_time);
    return 1;
}

// Global.h 装备位置枚举 → Lua 全局常量
static const LuaEnumEntry kWieldPosEnums[] = {
    {"WIELD_NONE", WIELD_NONE},
    {"WIELD_HEAD", WIELD_HEAD},
    {"WIELD_LEFT_ERR", WIELD_LEFT_ERR},
    {"WIELD_RIGHT_ERR", WIELD_RIGHT_ERR},
    {"WIELD_ERR", WIELD_ERR},
    {"WIELD_LEFT_HAND", WIELD_LEFT_HAND},
    {"WIELD_RIGHT_HAND", WIELD_RIGHT_HAND},
    {"WIELD_HAND", WIELD_HAND},
    {"WIELD_LEFT_WRIST", WIELD_LEFT_WRIST},
    {"WIELD_RIGHT_WRIST", WIELD_RIGHT_WRIST},
    {"WIELD_WRIST", WIELD_WRIST},
    {"WIELD_ARMOR", WIELD_ARMOR},
    {"WIELD_NECK", WIELD_NECK},
    {"WIELD_WAIST", WIELD_WAIST},
    {"WIELD_BOOT", WIELD_BOOT},
    {"WIELD_MANTLE", WIELD_MANTLE},
    {"WIELD_ARMOR_HANDS", WIELD_ARMOR_HANDS},
    {"WIELD_BOTH_HAND", WIELD_BOTH_HAND},
    {"WIELD_RING", WIELD_RING},
    {"WIELD_CLOTH", WIELD_CLOTH},
    {nullptr, 0},
};

void lua_bind_reg_wield_enums(lua_State* L)
{
    lua_bind_reg_enums(L, kWieldPosEnums);
}

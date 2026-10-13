/**
 * @file GameInit.cpp
 * @brief 游戏初始化 / 关闭 / 服务器配置
 */

#include "GameInit.h"
#include "../object/Global.h"
#include "Channel.h"
#include "LuaEngine.h"
#include "LuaBind.h"
#include "LuaHook.h"
#include "../util/CreateDump.h"
#include "../util/Encoding.h"
#include "IniParser.h"
#include "../util/Logger.h"
#include "ItemMgr.h"
#include "../config/PartyConfig.h"
#include "../config/ServerConfig.h"

#include <process.h>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>

#include "../db/Sql.h"

//---------------------------------------------------------
// 服务器配置变量定义
//---------------------------------------------------------
// 生产 ItemMgr 实例
static std::unique_ptr<IItemMgr> s_ItemMgrOwner;

int g_server_id          = 1;
int g_server_port        = 6600;
char g_server_name[64]   = "金剑江湖";
int g_server_charge      = 0;
char g_server_dbname[64] = "game";
char g_start_room[128]   = "佛山北帝庙";
int g_ip_limit           = 30;

//---------------------------------------------------------
// 时间变量定义
//---------------------------------------------------------
int g_current_time;         // 当前时间
int g_heart_beat_flag = 1;  // 心跳标志
int g_boot_time;            // 启动时间

//---------------------------------------------------------
// 初始化 Lua 脚本引擎
//---------------------------------------------------------
void init_lua_engine()
{
    LOG_TRACE("Lua engine init start...");

    const char* luaRoot     = ini_get("lua", "script_root", "lua");
    const char* defaultRoot = ini_get("lua", "default_script_root", "");
    LOG_DEBUG("Lua script root: %s", luaRoot);
    if (defaultRoot && defaultRoot[0])
        LOG_DEBUG("Lua default script root: %s", defaultRoot);

    if (!g_LuaEngine->init(luaRoot, defaultRoot)) {
        LOG_FATAL("Lua engine init failed!");
        return;
    }
    lua_bind_init(g_LuaEngine->get_state());
    // 加载钩子/命令脚本（script_root 优先，回退 default_script_root）
    lua_hook_boot(g_LuaEngine->get_state());
    // 加载 Lua 版 damage_msg（全局函数，替代 C++ 实现）
    // 优先 defaultRoot（服务器自带默认脚本），找不到再回退到
    // luaRoot（用户脚本）
    {
        lua_State* L = g_LuaEngine->get_state();
        const char* roots[2]
            = {defaultRoot ? defaultRoot : "", luaRoot ? luaRoot : ""};
        bool loaded = false;
        for (int i = 0; i < 2 && !loaded; i++) {
            if (!roots[i][0]) continue;
            std::string dmgPath = std::string(roots[i]) + "/damage_msg.lua";
            std::replace(dmgPath.begin(), dmgPath.end(), '\\', '/');
            if (luaL_dofile(L, dmgPath.c_str()) == LUA_OK) {
                loaded = true;
            }
            else {
                const char* err = lua_tostring(L, -1);
                lua_pop(L, 1);
                LOG_DEBUG("damage_msg.lua not in %s: %s", roots[i], err);
            }
        }
        if (!loaded) {
            LOG_ERROR(
                "Failed to load damage_msg.lua from both "
                "default and user script roots.");
        }
    }
    LOG_TRACE("Lua engine initialized.");
}

//---------------------------------------------------------
// 初始化游戏
//---------------------------------------------------------
int init_game(const char* ipaddress, const char* name, const char* password)
{
    CCreateDump::instance()->declar_dump_file("dumpfile");
    s_ItemMgrOwner = create_item_mgr();
    g_item_mgr     = s_ItemMgrOwner.get();

    // 从统一配置文件读入服务器配置
    g_server_port        = ini_get_int("server", "port", 6600);
    std::string tmp_name = utf8_to_gbk(ini_get("server", "name", "jianghu"));
    strncpy(g_server_name, tmp_name.c_str(), sizeof(g_server_name) - 1);
    g_server_id     = ini_get_int("server", "id", 1);
    g_server_charge = ini_get_int("server", "charge", 0);
    strncpy(g_server_dbname, ini_get("database", "name", "game"),
            sizeof(g_server_dbname) - 1);
    std::string tmp_room
        = utf8_to_gbk(ini_get("server", "start_room", "佛山北帝庙"));
    strncpy(g_start_room, tmp_room.c_str(), sizeof(g_start_room) - 1);

    g_ip_limit = ini_get_int("server", "ip_limit", 30);

    logger_set_level(logger_level_from_str(ini_get("log", "level", "trace")));
    logger_set_path(ini_get("log", "path", "log"));

    // 初始化组队配置
    CPartyConfig::init();

    // 初始化玩法开关
    CServerConfig::init();

    srand((unsigned)time(NULL));
    g_current_time = g_boot_time = static_cast<int>(time(NULL));

    // Lua 引擎必须在 SQL 之前初始化，因为 Restore_Data 会调用 Create_Item，
    // 后者依赖 Lua 脚本引擎查找物品定义。
    init_lua_engine();

    if (!init_sql_server(ipaddress, name, password)) return 0;
    g_channel.create();

    LOG_TRACE("Game Initialized.");
    return 1;
}

//---------------------------------------------------------
// 释放子系统（声明在各自模块）
//---------------------------------------------------------
void heart_beat_release();
void user_release();
void npc_release();
void room_release();
void skill_release();
void condition_release();

void exit_game()
{
    heart_beat_release();
    user_release();
    npc_release();
    g_item_mgr->item_release();
    room_release();
    skill_release();
    condition_release();
    g_item_mgr = nullptr;
    s_ItemMgrOwner.reset();
}

void dis_connect_database() { disconnect_sql_server(); }

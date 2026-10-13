/**
 * @file CharSave.cpp
 * @brief implementation of CChar (CharSave). */

#include <windows.h>

#include <vector>

#include "Global.h"
#include "CReward.h"
#include "Char.h"
#include "User.h"
#include "Npc.h"
#include "Skill.h"
#include "CmdHandle.h"
#include "Room.h"
#include "Condition.h"
#include "Channel.h"
#include "Combatd.h"
#include "CombatCalc.h"
#include "CombatAnnounce.h"
#include "AutoFight.h"
#include "CombatConfig.h"
#include "Money.h"
#include "FightRoom.h"
#include "Corpse.h"

#include "Sql.h"
#include "SqlEscape.h"

extern char g_dbip[80], g_user[80], g_pwd[80];

int CChar::save_to_server()
{
    if (m_destruct) return 0;

    // 游客（guest/guest 进大厅的临时角色）没有正式档案，不落库
    if (query("guest")) return 1;

    // 掌门经验的存盘
    int32_t combat_exp = query_exp();
    if (combat_exp < 50000)  // 50000以下才能给师父掌门经验
    {
        int32_t addexp = query_exp() - query("family/last_exp");
        if (query("family/user")) {
            // 每100点，10000以下2点，50000以下1点
            if (combat_exp < 10000) addexp *= 2;

            char master[40];

            strncpy(master, querystr("family/master_id"), 40);
            char key[80];

            CMapping* ptr = g_channel.query_recruit(master);
            if (ptr)
                addexp
                    += ptr->query(snprintf(key, 80, "%s/exp", querystr("id")));

            g_channel.update_recruit(master, id(1), name(1), addexp, 1);

            // 将入门经验清成当前经验。
            set("family/last_exp", query_exp());
        }
    }

    CMapping savedata;
    get_save_data(&savedata);

    const char* attr = savedata.get_buffer();
    CContainer* coin = present_name("coin", IS_ITEM);
    int32_t money    = query("balance");
    if (coin) money += coin->query_amount();

    char logintime[80], savetime[80];
    time_t current = query("login/time");
    struct tm* newtime;
    newtime = localtime(&current);

    sprintf(logintime, "%d/%02d/%02d %02d:%02d:%02d", 1900 + newtime->tm_year,
            newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour,
            newtime->tm_min, newtime->tm_sec);

    current = g_current_time;
    newtime = localtime(&current);

    sprintf(savetime, "%d/%02d/%02d %02d:%02d:%02d", 1900 + newtime->tm_year,
            newtime->tm_mon + 1, newtime->tm_mday, newtime->tm_hour,
            newtime->tm_min, newtime->tm_sec);

    // cc_savedata 是 JSON 文本，含反斜杠与单引号；连接已设
    // NO_BACKSLASH_ESCAPES，只需对各字符串字段翻倍单引号。改用 std::string
    // 动态拼接，避免固定缓冲区对大存档的截断风险。
    std::string msg = "update jianghudb set cc_name='";
    msg += sql_escape_literal(name(1));
    msg += "', cc_str=";
    msg += std::to_string(query("str"));
    msg += ", cc_int=";
    msg += std::to_string(query("int"));
    msg += ", cc_con=";
    msg += std::to_string(query("con"));
    msg += ", cc_dex=";
    msg += std::to_string(query("dex"));
    msg += ", cc_family='";
    msg += sql_escape_literal(querystr("family/family_name"));
    msg += "', cc_age=";
    msg += std::to_string(query("age"));
    msg += ", cc_gender='";
    msg += sql_escape_literal(querystr("gender"));
    msg += "', cc_master='";
    msg += sql_escape_literal(querystr("family/master_name"));
    msg += "', cc_nickname='";
    msg += sql_escape_literal(querystr("nickname"));
    msg += "', cc_rank='";
    msg += sql_escape_literal(query_rank(this));
    msg += "', cc_class='";
    msg += sql_escape_literal(querystr("class"));
    msg += "', cc_level=";
    msg += std::to_string(query("level"));
    msg += ", cc_ip='";
    msg += sql_escape_literal(get_ip(this));
    msg += "', cc_balance=";
    msg += std::to_string(money);
    msg += ", cc_logintime='";
    msg += sql_escape_literal(logintime);
    msg += "', cc_savetime='";
    msg += sql_escape_literal(savetime);
    msg += "', cc_experice=";
    msg += std::to_string(query_exp());
    msg += ", cc_repute=";
    msg += std::to_string(query("repute"));
    msg += ", cc_grant=";
    msg += std::to_string(query("grant"));
    msg += ", cc_savedata='";
    msg += sql_escape_literal(attr);
    msg += "' where cc_id=";
    msg += id(1);

    if (g_db->execute(msg.c_str()) != 0) {
        LOG_ERROR("Failed to save, %s", g_db->error());
        return 0;
    }

    return 1;
}

// 存盘,暂存
int CChar::save()
{
    if (!query_attr(CAN_SAVE)) return 0;

    if (m_destruct) return 0;

    if (!userp(this)) {
        CMapping savedata;
        get_save_data(&savedata);
        return CContainer::save_impl(&savedata);
    }

    // 如果暂存时间超过一个小时,则永久存盘一次
    if (g_current_time > query_temp("save_to_server")) {
        save_to_server();
        set_temp("save_to_server", g_current_time + 3600);
    }

    return save_to_server();
}

// 获得存盘数据
int CChar::get_save_data(CMapping* save)
{
    save->set("dbase", _M);
    save->set("skills", skills);
    save->set("learned", learned);
    save->set("skill_map", skill_map);
    save->set("skill_prepare", skill_prepare);

    if (userp(this)) {
        CMapping auto_load;
        CContainer* obj;
        char msg[80];
        int i = 0;

        for (auto val : m_item_list) {
            obj = val;
            // 非可自动装载物件不予存盘
            auto_load.set(
                snprintf(msg, 80, "%s,%d:%d,%s", obj->querystr("base_name"),
                         obj->query("index"), i++, obj->querystr("unique_id")),
                obj->query_amount());

            obj->save();
        }

        save->set("auto_load", auto_load);
    }

    return 1;
}

// 恢复存盘数据
int CChar::restore_data(CMapping* save)
{
    _M.copy_mapping(save->querymap("dbase"));

    skills.copy_mapping(save->querymap("skills"));
    learned.copy_mapping(save->querymap("learned"));
    skill_map.copy_mapping(save->querymap("skill_map"));
    skill_prepare.copy_mapping(save->querymap("skill_prepare"));

    CContainer* obj;
    CVector v;
    CMapping auto_load;

    auto_load.copy_mapping(save->querymap("auto_load"));
    auto_load.keys(v);

    set("infinity", 1);

    char str[80];

    for (int i = 0; i < v.count(); i++) {
        strncpy(str, v[i], 80);

        char* ptr = strstr(str, ",");
        if (ptr) {
            (*ptr) = 0;

            char* ptr2 = strstr(ptr + 1, ":");
            if (ptr2) (*ptr2) = 0;

            char* ptr3      = ptr2 ? strstr(ptr2 + 1, ",") : nullptr;
            char* unique_id = "";
            if (ptr3) {
                (*ptr3)   = 0;
                unique_id = ptr3 + 1;
            }

            obj = g_item_mgr->load_item(str, unique_id);
            obj->create(atol(ptr + 1));
            obj->setup();
        }
        else
            obj = g_item_mgr->load_item(str);

        obj->set_amount(auto_load[v[i]]);
        obj->move(this);
    }

    del("infinity");
    return 1;
}

// 从服务器装载
int CChar::load_from_server()
{
    CMapping savedata;

    char szQuery[512];
    DbResultSet result;

    snprintf(szQuery, 512, "select cc_savedata from jianghudb where cc_id=%s",
             querystr("id"));
    // 使用新的数据库接口
    std::string savedata_str;
    if (g_db->query(szQuery, result) != 0) {
        LOG_ERROR("query db fail! : %s", g_db->error());
        return false;
    }

    if (result.empty()) {
        LOG_ERROR("database fail: store null");
        return false;
    }
    if (result.rows[0].columns.empty()) {
        return false;
    }

    const DbRow& row = result.rows[0];
    if (row.columns.empty() || row[0].empty()) {
        return false;
    }

    savedata.load_buffer(row[0].c_str());

    if (!restore_data(&savedata)) return 0;
    return 1;
}

int CChar::load()
{
    if (!userp(this)) {
        CMapping save;
        if (!CContainer::load_impl(&save)) return 0;

        return restore_data(&save);
    }
    else {
        return load_from_server();
    }

    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// self:LoadFromServer() -> number - 从服务器加载该角色数据（1 成功 / 0 失败）
int l_load_from_server(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->load_from_server());
    return 1;
}

// self:SaveToServer() - 保存角色数据到服务器
int l_save_to_server(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CChar* ch = dynamic_cast<CChar*>(obj);
    if (!ch) return 0;
    lua_pushinteger(L, ch->save_to_server());
    return 1;
}

// self:Offline() - 强制玩家下线
int l_offline(lua_State* L)
{
    CMainObject* obj = get_object(L, 1);
    if (!obj) return 0;
    CUser* u = dynamic_cast<CUser*>(obj);
    if (!u) return 0;
    LOG_INFO("Lua脚本强制下线: %s(id:%s)", u->name(1), u->id(1));
    u->offline();
    return 0;
}

#include <windows.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "Combatd.h"
#include "Channel.h"
#include "Global.h"
#include "User.h"

#include "IDatabase.h"
#include "Sql.h"
#include "IniParser.h"

// 脚本开机经 db_register_ddl 注册的建表语句（C++ 对其内容零知识）
std::vector<std::string> g_dataset_table_ddl;

namespace {

// SQLite 与 MySQL 的建表方言差异：
//   - SQLite 不认 engine/charset 表选项与内联 KEY 索引（索引另建）；
//   - 自增主键在 SQLite 里必须是精确的 INTEGER PRIMARY KEY（rowid 别名），
//     写成 int(32) AUTO_INCREMENT 会被当作普通整型而不再自动分配。
//     jianghudb 额外带 AUTOINCREMENT 关键字：账号 ID 起点存放在
//     sqlite_sequence，无此关键字则该表不存在、起点无处可播。
// 列类型（int/varchar/tinyint/blob/datetime）与反引号两后端都接受。
bool backend_is_sqlite() { return g_db != nullptr && g_db->is_sqlite(); }

// 组装建表语句。pk 为主键子句（不含前导逗号；自增表在 SQLite 下已内联，
// 传空串）；keys 为 MySQL 额外索引子句（SQLite 忽略，纯性能项）。
std::string BuildCreateTable(const char* name, const std::string& columns,
                             const char* pk, const char* keys)
{
    const bool sqlite = backend_is_sqlite();
    std::string sql   = "CREATE TABLE IF NOT EXISTS `";
    sql += name;
    sql += "` (";
    sql += columns;
    if (pk && pk[0]) {
        sql += ",";
        sql += pk;
    }
    if (!sqlite && keys && keys[0]) {
        sql += ",";
        sql += keys;
    }
    sql += sqlite ? ");" : ")engine=InnoDB default charset=utf8;";
    return sql;
}

// 自增主键列定义（各表首列）
std::string AutoPkColumn(const char* mysql_def, const char* sqlite_def)
{
    return backend_is_sqlite() ? sqlite_def : mysql_def;
}

// jianghudb 是否已存在——账号 ID 起点只在首建时播种，重跑
// prepare_tables 绝不能重置自增计数器（否则新号撞已发的 cc_id）。
bool game_table_exists()
{
    DbResultSet result;
    const char* sql
        = backend_is_sqlite()
              ? "SELECT name FROM sqlite_master WHERE type='table' "
                "AND name='jianghudb'"
              : "SELECT table_name FROM information_schema.tables "
                "WHERE table_schema=DATABASE() AND table_name='jianghudb'";
    if (g_db->query(sql, result) != 0) {
        LOG_ERROR("Prepare_tables: query jianghudb existence fail! %s",
                  g_db->error());
        return true;  // 探测失败按已存在处理：宁可不播种，不可错播种
    }
    return !result.empty();
}

// 把 jianghudb 的自增起点抬到 1000（1~999 预留给巫师/系统账号）。
// 仅在 game_table_exists() 为假（首建）时调用。
bool seed_first_id()
{
    const char* sql = backend_is_sqlite()
                          ? "INSERT INTO sqlite_sequence (name, seq) VALUES "
                            "('jianghudb', 999)"
                          : "ALTER TABLE jianghudb AUTO_INCREMENT = 1000";
    if (g_db->execute(sql) != 0) {
        LOG_ERROR("Prepare_tables: seed id floor fail! %s", g_db->error());
        return false;
    }
    return true;
}

}  // namespace

bool prepare_tables(void)
{
    try {
        const bool sqlite = backend_is_sqlite();

        // t_item / t_npc / t_room / t_box 同构
        const std::string container_cols
            = "`obj_id` varchar(40) NOT NULL DEFAULT '',"
              "`obj_name` varchar(40) NOT NULL DEFAULT '',"
              "`obj_data` blob,"
              "`obj_lock` tinyint(1) NOT NULL DEFAULT '0'";

        std::vector<std::string> tables;

        // 账号主表
        tables.push_back(BuildCreateTable(
            "jianghudb",
            AutoPkColumn("`cc_id` int(32) NOT NULL AUTO_INCREMENT,",
                         "`cc_id` INTEGER PRIMARY KEY AUTOINCREMENT,")
                + "`cc_name` varchar(40) NOT NULL DEFAULT '',"
                  "`cc_passwd` varchar(22) NOT NULL DEFAULT '',"
                  "`cc_email` varchar(64) NOT NULL DEFAULT '',"
                  "`cc_str` int(16) NOT NULL DEFAULT '0',"
                  "`cc_int` int(16) NOT NULL DEFAULT '0',"
                  "`cc_con` int(16) NOT NULL DEFAULT '0',"
                  "`cc_dex` int(16) NOT NULL DEFAULT '0',"
                  "`cc_family` varchar(20) DEFAULT NULL,"
                  "`cc_age` int(8) NOT NULL DEFAULT '0',"
                  "`cc_gender` varchar(4) NOT NULL DEFAULT '',"
                  "`cc_master` varchar(40) DEFAULT NULL,"
                  "`cc_nickname` varchar(40) DEFAULT NULL,"
                  "`cc_rank` varchar(40) DEFAULT NULL,"
                  "`cc_class` varchar(16) DEFAULT NULL,"
                  "`cc_level` tinyint(3) NOT NULL DEFAULT '0',"
                  "`cc_experice` int(64) DEFAULT '0',"
                  "`cc_savedata` blob,"
                  "`cc_remark` int(16) NOT NULL DEFAULT '0',"
                  "`cc_repute` int(16) NOT NULL DEFAULT '0',"
                  "`cc_grant` tinyint(3) NOT NULL DEFAULT '30',"
                  "`cc_score` int(16) NOT NULL DEFAULT '0',"
                  "`cc_server` tinyint(3) NOT NULL DEFAULT '0',"
                  "`cc_verifypass` varchar(20) NOT NULL DEFAULT '',"
                  "`cc_ip` varchar(80) NOT NULL DEFAULT '',"
                  "`cc_balance` int(16) NOT NULL DEFAULT '0',"
                  "`cc_logintime` datetime NOT NULL DEFAULT "
                  "'0000-00-00 00:00:00',"
                  "`cc_savetime` datetime NOT NULL DEFAULT "
                  "'0000-00-00 00:00:00',"
                  "`cc_freeze` tinyint(1) NOT NULL DEFAULT '0'",
            sqlite ? "" : "PRIMARY KEY (`cc_id`)",
            "KEY `cc_id` (`cc_id`),KEY `cc_grant` (`cc_grant`),"
            "KEY `cc_server` (`cc_server`),KEY `cc_experice` (`cc_experice`),"
            "KEY `ages` (`cc_age`),KEY `balances` (`cc_balance`),"
            "KEY `reputes` (`cc_repute`)"));

        // 门派招募
        tables.push_back(
            BuildCreateTable("recruit_exp",
                             "`re_master` int(16) NOT NULL DEFAULT '0',"
                             "`re_id` int(16) NOT NULL DEFAULT '0',"
                             "`re_name` char(40) NOT NULL DEFAULT '',"
                             "`re_exp` int(16) NOT NULL DEFAULT '0',"
                             "`re_apprentice` tinyint(1) NOT NULL DEFAULT '0'",
                             "PRIMARY KEY (`re_master`,`re_id`)",
                             "KEY `re_master` (`re_master`,`re_id`)"));

        // 容器/物品/房间/箱子
        for (const char* name : {"t_item", "t_npc", "t_room", "t_box"}) {
            tables.push_back(BuildCreateTable(name, container_cols,
                                              "PRIMARY KEY (`obj_id`)",
                                              "KEY `obj_id` (`obj_id`)"));
        }

        // 玩家存档
        tables.push_back(BuildCreateTable(
            "t_saveobj",
            "`owner` int(16) NOT NULL DEFAULT '0',"
            "`base_name` varchar(40) NOT NULL DEFAULT '',"
            "`savedata` blob NOT NULL,"
            "`grid` tinyint(2) NOT NULL DEFAULT '0'",
            "PRIMARY KEY (`grid`,`owner`)",
            "KEY `owner` (`owner`),KEY `base_name` (`base_name`)"));

        // 旧式注册表
        tables.push_back(BuildCreateTable(
            "user",
            AutoPkColumn("`id` int(16) NOT NULL AUTO_INCREMENT,",
                         "`id` INTEGER PRIMARY KEY,")
                + "`name` varchar(40) NOT NULL DEFAULT '',"
                  "`passwd` varchar(10) NOT NULL DEFAULT '0',"
                  "`email` varchar(64) NOT NULL DEFAULT '',"
                  "`icqnumber` int(16) DEFAULT NULL,"
                  "`phone` varchar(32) DEFAULT NULL,"
                  "`handset` varchar(32) DEFAULT NULL,"
                  "`bb` varchar(32) DEFAULT NULL,"
                  "`homepage` varchar(128) DEFAULT NULL,"
                  "`casehistory` varchar(255) DEFAULT NULL",
            sqlite ? "" : "PRIMARY KEY (`id`)", "KEY `id` (`id`)"));

        // 玩家历练记录
        tables.push_back(BuildCreateTable(
            "t_record",
            AutoPkColumn("`id` int(16) NOT NULL AUTO_INCREMENT,",
                         "`id` INTEGER PRIMARY KEY,")
                + "`cc_time` datetime NOT NULL DEFAULT "
                  "'0000-00-00 00:00:00',"
                  "`cc_id` int(16) NOT NULL DEFAULT '0',"
                  "`cc_type` int(16) NOT NULL DEFAULT '0',"
                  "`cc_level` int(16) NOT NULL DEFAULT '0',"
                  "`cc_savedata` blob",
            sqlite ? "" : "PRIMARY KEY (`id`)", "KEY `id` (`id`)"));

        // 社交关系（db_query_relative / db_update_relative 使用）
        tables.push_back(BuildCreateTable(
            "relative",
            "`cc_id` int(16) NOT NULL DEFAULT '0',"
            "`cc_other` int(16) NOT NULL DEFAULT '0',"
            "`cc_point` int(64) NOT NULL DEFAULT '0',"
            "`cc_relative` int(16) NOT NULL DEFAULT '0'",
            "PRIMARY KEY (`cc_id`,`cc_other`)", "KEY `cc_id` (`cc_id`)"));

        // 首建探测必须先于建表执行
        const bool first_creation = !game_table_exists();

        for (const std::string& ddl : tables) {
            if (g_db->execute(ddl.c_str()) != 0) {
                LOG_ERROR("Prepare_tables: fail! %s", ddl.c_str());
                return false;
            }
        }

        // 首次建表时播种账号 ID 起点；已有库跳过，避免重置计数器
        if (first_creation && !seed_first_id()) {
            return false;
        }

        // 数据集专用表：脚本注册的 DDL 逐条执行（表名/结构全归脚本）
        for (const std::string& ddl : g_dataset_table_ddl) {
            if (g_db->execute(ddl.c_str()) != 0) {
                LOG_ERROR("Prepare_tables: dataset table fail! %s",
                          ddl.c_str());
                return false;
            }
        }

        return true;
    } catch (...) {
        LOG_ERROR("Restore_Data:unknown!");
        return false;
    }
}

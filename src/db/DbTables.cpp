#include <windows.h>

#include <stdio.h>

#include <stdlib.h>

#include <set>
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

// 账号口令凭据列：登录/改密把盐与派生哈希写进这两对列，明文列只作为
// 历史数据的比对来源（见 docs/design/encryption.md）。
// 建表与历史库补列共用这份定义；列一律排在表末——verify_user 按
// select * 的位置读取既有列，插在中间会让建档字段整体错位。
struct CredentialColumn {
    const char* name;
    const char* type_def;
};

const CredentialColumn kCredentialColumns[] = {
    {"cc_passwd_salt", "varchar(32) NOT NULL DEFAULT ''"},
    {"cc_passwd_hash", "varchar(64) NOT NULL DEFAULT ''"},
    {"cc_verifypass_salt", "varchar(32) NOT NULL DEFAULT ''"},
    {"cc_verifypass_hash", "varchar(64) NOT NULL DEFAULT ''"},
};

// 建表用的列片段：`name` type_def,...（不含首尾逗号）
std::string CredentialColumnsDdl()
{
    std::string sql;
    for (const CredentialColumn& col : kCredentialColumns) {
        if (!sql.empty()) sql += ",";
        sql += "`";
        sql += col.name;
        sql += "` ";
        sql += col.type_def;
    }
    return sql;
}

// 历史库补齐凭据列：CREATE TABLE IF NOT EXISTS 不会为已有表补列，启动时
// 按列名探测，缺失的才 ALTER 追加。探测不到列（查询失败）即启动失败——
// 缺列时注册与登录都会在运行期报错，不如在开机阶段暴露。
bool ensure_credential_columns()
{
    DbResultSet rs;
    const char* probe
        = backend_is_sqlite()
              ? "PRAGMA table_info(jianghudb)"
              : "SELECT column_name FROM information_schema.columns "
                "WHERE table_schema=DATABASE() AND table_name='jianghudb'";
    if (g_db->query(probe, rs) != 0) {
        LOG_ERROR("Prepare_tables: probe jianghudb columns fail! %s",
                  g_db->error());
        return false;
    }

    // 列名位置：PRAGMA 行为 (cid,name,type,...)，information_schema 为单列
    const int name_idx = backend_is_sqlite() ? 1 : 0;
    std::set<std::string> existing;
    for (const DbRow& row : rs.rows) {
        if ((int)row.columns.size() > name_idx) existing.insert(row[name_idx]);
    }

    for (const CredentialColumn& col : kCredentialColumns) {
        if (existing.count(col.name)) continue;
        const std::string sql
            = std::string("ALTER TABLE jianghudb ADD COLUMN `") + col.name
              + "` " + col.type_def;
        if (g_db->execute(sql.c_str()) != 0) {
            LOG_ERROR("Prepare_tables: add column %s fail! %s", col.name,
                      g_db->error());
            return false;
        }
    }
    return true;
}

// 账号 ID 起点：1~999 预留给巫师/系统账号，首个玩家账号从 1000 起发。
constexpr long ACCOUNT_ID_FLOOR = 1000;

// 已发的最大账号 ID；查询失败返回 -1，调用方按「需要播种」处理。
long max_account_id()
{
    DbResultSet rs;
    if (g_db->query("SELECT MAX(cc_id) FROM jianghudb", rs) != 0) {
        LOG_ERROR("Prepare_tables: read max cc_id fail! %s", g_db->error());
        return -1;
    }
    if (rs.empty() || rs.rows[0].columns.empty() || rs.rows[0][0].empty()) {
        return -1;  // 空表（MAX 为 NULL）同样走播种
    }
    return atol(rs.rows[0][0].c_str());
}

// 起点播种语句（按执行顺序），方言差异只在这一个函数里：
//   MySQL  —— 低于当前值的 AUTO_INCREMENT 被服务端忽略，天然只抬不降；
//   SQLite —— 起点存 sqlite_sequence，首建时该行尚不存在，先补行，
//             再按 MAX(seq, 999) 抬高（行不存在时 UPDATE 为空操作）。
std::vector<const char*> id_floor_seed_sqls()
{
    if (!backend_is_sqlite()) {
        return {"ALTER TABLE jianghudb AUTO_INCREMENT = 1000"};
    }
    return {
        "INSERT INTO sqlite_sequence (name, seq) SELECT 'jianghudb', 999 "
        "WHERE NOT EXISTS (SELECT 1 FROM sqlite_sequence WHERE "
        "name='jianghudb')",
        "UPDATE sqlite_sequence SET seq = MAX(seq, 999) WHERE "
        "name='jianghudb'",
    };
}

// 把自增起点抬到 ACCOUNT_ID_FLOOR。调用前提：已发号不足起点。
// 逐条执行方言播种语句，任一失败即中止启动——起点没播下去就建号，
// 新号会落进巫师/系统预留段。
bool raise_id_floor()
{
    for (const char* sql : id_floor_seed_sqls()) {
        if (g_db->execute(sql) != 0) {
            LOG_ERROR("Prepare_tables: seed id floor fail! %s", g_db->error());
            return false;
        }
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
                  "`cc_freeze` tinyint(1) NOT NULL DEFAULT '0',"
                // 凭据列固定排在末尾：verify_user 按 select * 位置读列
                + CredentialColumnsDdl(),
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

        // 容器/物品/房间/箱子：统一容器表结构，经通用容器接口按对象
        // 类型选表访问（obj_id 主键）
        for (const char* name : {"t_item", "t_npc", "t_room", "t_box"}) {
            tables.push_back(BuildCreateTable(name, container_cols,
                                              "PRIMARY KEY (`obj_id`)",
                                              "KEY `obj_id` (`obj_id`)"));
        }

        // 社交关系（db_query_relative / db_update_relative 使用）
        tables.push_back(BuildCreateTable(
            "relative",
            "`cc_id` int(16) NOT NULL DEFAULT '0',"
            "`cc_other` int(16) NOT NULL DEFAULT '0',"
            "`cc_point` int(64) NOT NULL DEFAULT '0',"
            "`cc_relative` int(16) NOT NULL DEFAULT '0'",
            "PRIMARY KEY (`cc_id`,`cc_other`)", "KEY `cc_id` (`cc_id`)"));

        for (const std::string& ddl : tables) {
            if (g_db->execute(ddl.c_str()) != 0) {
                LOG_ERROR("Prepare_tables: fail! %s", ddl.c_str());
                return false;
            }
        }

        // 账号 ID 起点：1~999 预留段是每次开机都要满足的不变量，不是建表
        // 那一刻的一次性动作——只播一次的话，起点一旦回落（服务端重载后按
        // MAX(cc_id)+1 重建计数器）或历史库本就没播，新号就落进巫师/系统
        // 预留段。播种只抬不降：误播种不伤害已发的 cc_id，漏播种才会出事。
        if (max_account_id() < ACCOUNT_ID_FLOOR && !raise_id_floor()) {
            return false;
        }

        // 账号口令凭据列：历史库补齐（新库建表已含，探测后无动作）
        if (!ensure_credential_columns()) {
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

/**
 * @file SqlConfig.cpp
 * @brief 数据库类型（[database] type）的读取。
 *
 * 与 Sql.cpp 分文件编译：Sql.cpp 因建库/连接依赖被排除出 jianghu_core.lib
 * （测试由 tests/unit/mock/mock_db.cpp 提供替身），而库类型是纯配置读取——
 * 开机登记建表语句的脚本（data/card_core.init_table 等）在数据库尚未连接时
 * 就要按方言选 DDL，因此它必须留在库里、并可被单测直接覆盖。
 */

#include "IniParser.h"
#include "Sql.h"

const char* get_db_type() { return ini_get("database", "type", "mysql"); }

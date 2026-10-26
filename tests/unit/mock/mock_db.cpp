// mock_db.cpp — Mock implementations for database functions
//
// Replaces: db/Sql.cpp (excluded from jianghu_core.lib)
// All db_* functions return success/failure based on g_mock_db_succeed flag.
//
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "IDatabase.h"

// ====================================================================
// Test control variables
// ====================================================================
bool g_mock_db_succeed = true;

// ====================================================================
// Global database instance (real code expects this)
// ====================================================================
class IDatabase;
IDatabase* g_db = nullptr;

// ====================================================================
// Database type
// ====================================================================
// get_db_type() 由库提供（src/db/SqlConfig.cpp，读 [database] type）：
// 它是纯配置读取，不属于 db/Sql.cpp 的替身范围。

// ====================================================================
// Server config (expected by some db functions)
// ====================================================================
long iTelLinHand    = 0;
double g_game_count = 0.0;

// ====================================================================
// Container/object/box: real implementations compiled from
//   SqlContainer.cpp, SqlAccount.cpp, SqlCharge.cpp
// ====================================================================

// ====================================================================
// Box storage (implemented in DbTables.cpp / other)
// ====================================================================
bool db_save_box(const char* obj_id, const char* obj_name, const char* obj_data,
                 const char* room_id, int grid)
{
    return g_mock_db_succeed;
}

bool db_load_box(const char* obj_id, std::string& out_data)
{
    out_data.clear();
    return g_mock_db_succeed;
}

bool db_delete_box(const char* obj_id) { return g_mock_db_succeed; }

// ====================================================================
// Server initialization stubs (called by Init_Game)
// ====================================================================
bool select_game_database() { return g_mock_db_succeed; }

bool init_sql_server(const char* ipaddress, const char* name,
                     const char* password)
{
    return g_mock_db_succeed;
}

bool restore_data_table() { return g_mock_db_succeed; }

bool verify_user(const char* userid, char* passwd) { return g_mock_db_succeed; }

bool ensure_wizard_accounts() { return g_mock_db_succeed; }

void disconnect_sql_server() {}

// ====================================================================
// Valid_Enter_Game — from Sql.cpp (excluded from tests)
// ====================================================================
class CUser;
double valid_enter_game(CUser* me, const char* cardno, const char* password)
{
    return 1.0;  // always succeed
}

// ====================================================================
// Additional Sql.cpp functions (needed by lib code)
// ====================================================================
class CUser;
class CContainer;

void on_user_out(CUser* me) {}

// ====================================================================
// g_ini_data (from main.cpp — excluded from lib)
// ====================================================================
std::map<std::string, std::map<std::string, std::string>> g_ini_data;

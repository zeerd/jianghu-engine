/**
 * @file SqlAccount.cpp
 * @brief 用户账号管理 + 频道/社交操作  */

// windows.h 必须最先包含：Global.h 有 using namespace std;，
// 后置引入 windows.h(rpcndr.h 的 byte)会与 std::byte 撞成 C2872。
#include <windows.h>

#include <stdio.h>

#include <string>
#include <vector>

#include "Crypto.h"
#include "Global.h"
#include "IDatabase.h"
#include "Sql.h"
#include "SqlEscape.h"
#include "LoginRegister.h"

// ============================================================
// 用户/账号操作 (对应 Login.cpp, User.cpp, Char.cpp)
// ============================================================

namespace {

// 口令的比对形态：登录口对整条登录串剥除单引号，所有写入与校验入口
// 必须先规范化，否则含引号口令存进去后与登录侧不一致（自我锁死）。
std::string normalized_password(const char* pass)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", pass ? pass : "");
    normalize_password(buf);
    return std::string(buf);
}

// 比对已落库的哈希：解码盐与哈希后重新派生，再定长比较。
bool matches_password_credential(const std::string& pass,
                                 const std::string& salt_b64,
                                 const std::string& hash_b64)
{
    std::vector<uint8_t> salt, expected;
    if (!base64_decode(salt_b64, salt)) return false;
    if (!base64_decode(hash_b64, expected)) return false;
    if (expected.size() != CRYPTO_HASH_LEN) return false;

    uint8_t derived[CRYPTO_HASH_LEN];
    if (!pbkdf2_hmac_sha256(pass.c_str(), salt.data(), salt.size(),
                            PBKDF2_ITERATIONS, derived, sizeof(derived)))
        return false;
    return constant_time_equal(derived, expected.data(), expected.size());
}

}  // namespace

// 暗码校验：哈希列非空时比对哈希；否则按历史存储形态在 SQL 侧等值比对
// （SQLite 存明文、MySQL 存 OLD_PASSWORD 弱哈希），通过后原地迁移为哈希
// 并清空旧列。未设置暗码的账号两列皆空，空入参与空存储等值——「空暗码
// 校验」依赖这条语义（新号首次设暗码前的口令确认走这里）。
// 暗码契约：单引号替换为空格，与写入侧同口径（主密码的契约是剥除）。
bool db_verify_password(const char* id, const char* pass)
{
    if (!g_db) return false;

    try {
        char msg[512], tmp[80];
        strncpy(tmp, pass ? pass : "", sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        for (char* p = tmp; *p; ++p) {
            if (*p == '\'') *p = ' ';
        }

        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_verifypass_salt, cc_verifypass_hash from "
                 "jianghudb where cc_id=%s",
                 id);
        if (g_db->query(msg, result) != 0) {
            LOG_ERROR("db_verify_password: query db fail! %s", g_db->error());
            return false;
        }
        if (result.empty() || result.rows[0].columns.size() < 2) return false;

        const std::string& salt = result.rows[0][0];
        const std::string& hash = result.rows[0][1];

        if (!hash.empty()) return matches_password_credential(tmp, salt, hash);

        // 历史形态等值比对（方言分支沿用原查询）
        if (g_db->is_sqlite()) {
            snprintf(msg, sizeof(msg),
                     "select cc_name from jianghudb where cc_id=%s and "
                     "cc_verifypass='%s'",
                     id, tmp);
        }
        else {
            snprintf(msg, sizeof(msg),
                     "select cc_name from jianghudb where cc_id=%s and "
                     "cc_verifypass=OLD_PASSWORD('%s')",
                     id, tmp);
        }
        if (g_db->query(msg, result) != 0) {
            LOG_ERROR("db_verify_password: query db fail! %s", g_db->error());
            return false;
        }
        if (result.empty()) return false;

        // 无感迁移：写入盐与哈希、清空旧列。校验已通过，迁移写失败只记
        // 日志，不改变本次结论。
        std::string new_salt, new_hash;
        if (!make_password_credential(tmp, new_salt, new_hash)) {
            LOG_ERROR("db_verify_password: derive key fail! id=%s", id);
            return true;
        }
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_verifypass='', "
                 "cc_verifypass_salt='%s', cc_verifypass_hash='%s' "
                 "where cc_id=%s",
                 new_salt.c_str(), new_hash.c_str(), id);
        if (g_db->execute(msg) != 0) {
            LOG_ERROR("db_verify_password: migrate fail! id=%s", id);
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_verify_password: unknown exception");
        return false;
    }
}

// 主密码校验（登录口与账号命令的身份核对都走这里）：哈希列非空时比对
// 哈希；否则按历史明文等值比对，通过后原地迁移为哈希并清空明文列。
// 明文列为空（迁移后的账号、或从未设过口令）不参与比对——迁移会把明文
// 列清成空串，若仍等值比对，空口令就能顶替真口令。
bool db_authenticate_account(const char* id, const char* passwd)
{
    if (!g_db) return false;

    try {
        const std::string pass = normalized_password(passwd);

        DbResultSet result;
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "select cc_passwd, cc_passwd_salt, cc_passwd_hash from "
                 "jianghudb where cc_id=%s",
                 id);
        if (g_db->query(msg, result) != 0) {
            LOG_ERROR("db_authenticate_account: query db fail! %s",
                      g_db->error());
            return false;
        }
        if (result.empty() || result.rows[0].columns.size() < 3) {
            LOG_ERROR("database fail: user:%s passwd:%s store null", id,
                      pass.c_str());
            return false;
        }

        const std::string& stored = result.rows[0][0];
        const std::string& salt   = result.rows[0][1];
        const std::string& hash   = result.rows[0][2];

        bool matched = false;
        if (!hash.empty()) {
            matched = matches_password_credential(pass, salt, hash);
        }
        else if (!stored.empty()) {
            matched = (stored == pass);
        }

        if (!matched) {
            // 账号不存在与口令不符同形：库里查不到与该口令匹配的凭据
            LOG_ERROR("database fail: user:%s passwd:%s store null", id,
                      pass.c_str());
            return false;
        }

        if (hash.empty()) {
            // 无感迁移：写入盐与哈希、清空明文列。口令本身已验证通过，
            // 迁移写失败只记日志，不阻断本次登录。
            std::string new_salt, new_hash;
            if (!make_password_credential(pass.c_str(), new_salt, new_hash)) {
                LOG_ERROR("db_authenticate_account: derive key fail! id=%s",
                          id);
                return true;
            }
            snprintf(msg, sizeof(msg),
                     "update jianghudb set cc_passwd='', cc_passwd_salt='%s', "
                     "cc_passwd_hash='%s' where cc_id=%s",
                     new_salt.c_str(), new_hash.c_str(), id);
            if (g_db->execute(msg) != 0) {
                LOG_ERROR("db_authenticate_account: migrate fail! id=%s", id);
            }
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_authenticate_account: unknown exception");
        return false;
    }
}

// 注册新账号：校验参数后向 jianghudb 插入一行基本资料（不含 cc_savedata，
// 正式档案由首次登录时 verify_user 的新人分支建立，与巫师账号自举同路径）。
// 成功返回新账号 id（>0）；失败返回 0，并把错误信息写入 err。
// 参数顺序：name, passwd, email, gender, str, intel, con, dex
long db_register_account(const char* name, const char* passwd,
                         const char* email, const char* gender, int str,
                         int intel, int con, int dex, char* err, int errlen)
{
    if (err && errlen > 0) err[0] = 0;

    if (!g_db) {
        if (err) snprintf(err, errlen, "数据库未就绪，请稍后再试。");
        return 0;
    }

    try {
        // 四维合法性：单项 10~30，总和 84（与 verify_user 一致）
        if (str < 10 || str > 30 || con < 10 || con > 30 || intel < 10
            || intel > 30 || dex < 10 || dex > 30) {
            if (err)
                snprintf(err, errlen, "天赋属性非法：单项须在 10 到 30 之间。");
            return 0;
        }
        if (str + con + intel + dex != 84) {
            if (err) snprintf(err, errlen, "天赋属性非法：四项总和必须为 84。");
            return 0;
        }

        // 名字校验（优先走 Lua 的 login.check_name）
        const char* name_err = check_legal_name(name);
        if (name_err && name_err[0]) {
            if (err) snprintf(err, errlen, "%s", name_err);
            return 0;
        }

        // 邮箱校验：简单格式检查（含 @ 且 @ 前后非空），不做查重
        if (!email || email[0] == '\0') {
            if (err) snprintf(err, errlen, "请填写邮箱地址。");
            return 0;
        }
        const char* at = strchr(email, '@');
        if (!at || at == email || at[1] == '\0') {
            if (err) snprintf(err, errlen, "邮箱格式不正确。");
            return 0;
        }

        // 密码：cc_passwd varchar(22)。密码契约不含单引号——登录口对整个
        // 登录串剥除单引号后才比对，注册必须同一规范化，否则含引号密码
        // 存库后与登录校验串不一致（自我锁死）；长度按规范化后的值校验。
        char pass[24];
        snprintf(pass, sizeof(pass), "%s", passwd ? passwd : "");
        normalize_password(pass);
        size_t plen = strlen(pass);
        if (plen < 4 || plen > 22) {
            if (err)
                snprintf(err, errlen, "密码长度必须在 4 到 22 个字符之间。");
            return 0;
        }

        // 性别
        if (strcmp(gender, "男性") != 0 && strcmp(gender, "女性") != 0) {
            if (err) snprintf(err, errlen, "性别只能是男性或女性。");
            return 0;
        }

        // 注意：不再查重名字，允许同名不同账号；cc_id 由数据库 AUTO_INCREMENT
        // 自动分配
        std::string esc_name  = sql_escape_literal(name);
        std::string esc_email = sql_escape_literal(email);

        // 凭据：明文列不写值，注册即写入盐与派生哈希（Base64 字符集不含
        // 引号，直接进字面量）。派生放在全部校验之后：口令非法时不白算。
        std::string cred_salt, cred_hash;
        if (!make_password_credential(pass, cred_salt, cred_hash)) {
            if (err) snprintf(err, errlen, "注册失败，请稍后再试。");
            return 0;
        }

        // 起手值列与建档（init_new_char）同源：年龄取 config 的 g_start_age、
        // 等级取 NEW_CHAR_LEVEL——在首次登录建档之前，账号行的年龄/等级就应是
        // 新角色的真实起手值，而不是列默认的 0。
        // 经验保持缺省 0；cc_savedata 仍按设计由首次登录的新人分支建立
        // （见 verify_user）。
        std::string sql
            = "insert into jianghudb (cc_name, cc_passwd, cc_passwd_salt, "
              "cc_passwd_hash, cc_email, cc_gender, cc_str, cc_int, cc_con, "
              "cc_dex, cc_age, cc_level) values ('"
              + esc_name + "', '', '" + cred_salt + "', '" + cred_hash + "', '"
              + esc_email + "', '" + sql_escape_literal(gender) + "', "
              + std::to_string(str) + ", " + std::to_string(intel) + ", "
              + std::to_string(con) + ", " + std::to_string(dex) + ", "
              + std::to_string(g_start_age) + ", "
              + std::to_string(NEW_CHAR_LEVEL) + ")";
        if (g_db->execute(sql.c_str()) != 0) {
            LOG_ERROR("db_register_account: insert fail! %s", g_db->error());
            if (err) snprintf(err, errlen, "注册失败，请稍后再试。");
            return 0;
        }

        // 取回自增生成的 cc_id
        DbResultSet id_result;
        long next_id         = 0;
        const char* id_query = g_db->is_sqlite() ? "SELECT last_insert_rowid()"
                                                 : "SELECT LAST_INSERT_ID()";
        if (g_db->query(id_query, id_result) == 0 && !id_result.empty()
            && !id_result.rows[0].columns.empty()) {
            next_id = atol(id_result.rows[0][0].c_str());
        }
        else {
            if (err) snprintf(err, errlen, "注册失败，无法获取账号 ID。");
        }
        return next_id;
    } catch (...) {
        LOG_ERROR("db_register_account: unknown exception");
        if (err) snprintf(err, errlen, "注册失败，请稍后再试。");
        return 0;
    }
}

bool db_change_password(const char* id, const char* new_pass)
{
    if (!g_db) return false;

    try {
        // 写路径契约：密码先规范化（剥单引号，与登录/注册同语义，否则
        // 含引号密码存进去登录时对不上），再派生盐与哈希落库；明文列清空。
        // 同一入口也覆盖巫师 chpass 这条不经命令层规范化的调用。
        const std::string pass = normalized_password(new_pass);
        std::string cred_salt, cred_hash;
        if (!make_password_credential(pass.c_str(), cred_salt, cred_hash)) {
            LOG_ERROR("db_change_password: derive key fail! id=%s", id);
            return false;
        }
        const std::string sql
            = "update jianghudb set cc_passwd='', cc_passwd_salt='" + cred_salt
              + "', cc_passwd_hash='" + cred_hash + "' where cc_id=" + id;
        int ret = g_db->execute(sql.c_str());
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_change_password: unknown exception");
        return false;
    }
}

bool db_change_verify_password(const char* id, const char* new_pass)
{
    if (!g_db) return false;

    try {
        // 暗码写路径：单引号替换为空格（与 LoginAuth 的调用口径一致、幂等），
        // 再派生盐与哈希落库；旧明文/弱哈希列清空。
        char tmp[80];
        strncpy(tmp, new_pass ? new_pass : "", sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;
        for (char* p = tmp; *p; ++p) {
            if (*p == '\'') *p = ' ';
        }

        std::string cred_salt, cred_hash;
        if (!make_password_credential(tmp, cred_salt, cred_hash)) {
            LOG_ERROR("db_change_verify_password: derive key fail! id=%s", id);
            return false;
        }
        const std::string sql
            = "update jianghudb set cc_verifypass='', cc_verifypass_salt='"
              + cred_salt + "', cc_verifypass_hash='" + cred_hash
              + "' where cc_id=" + id;
        int ret = g_db->execute(sql.c_str());
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_change_verify_password: unknown exception");
        return false;
    }
}

bool db_set_online_flag(const char* id, int online)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_server = %d where cc_id = %s", online,
                 id);
        int ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_set_online_flag: query db fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_set_online_flag: unknown exception");
        return false;
    }
}

int db_query_server(const char* id)
{
    if (!g_db) return -1;

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_server from jianghudb where cc_id = %s", id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_server: query db fail! %s", g_db->error());
            return -1;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return -1;
        }

        return atoi(result.rows[0][0].c_str());
    } catch (...) {
        LOG_ERROR("db_query_server: unknown exception");
        return -1;
    }
}

int db_query_freeze(const char* id)
{
    if (!g_db) return 1;  // 默认冻结

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_freeze from jianghudb where cc_id=%s", id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_freeze: query db fail! %s", g_db->error());
            return 1;
        }

        if (result.empty() || result.rows[0].columns.empty()
            || result.rows[0][0].empty()) {
            return 1;
        }

        return atoi(result.rows[0][0].c_str());
    } catch (...) {
        LOG_ERROR("db_query_freeze: unknown exception");
        return 1;
    }
}

bool db_set_freeze(const char* id, int freeze)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "update jianghudb set cc_freeze = %d where cc_id=%s", freeze,
                 id);
        int ret = g_db->execute(msg);
        return ret == 0;
    } catch (...) {
        LOG_ERROR("db_set_freeze: unknown exception");
        return false;
    }
}

// ============================================================
// 频道/社交操作 (对应 Channel.cpp)
// ============================================================

bool db_query_recruit(const char* master_id, DbResultSet& result)
{
    if (!g_db) return false;

    try {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "select re_id, re_name, re_exp, re_apprentice from "
                 "recruit_exp where re_master=%s",
                 master_id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_recruit: query db fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_query_recruit: unknown exception");
        return false;
    }
}

bool db_update_recruit(const char* master, const char* id, const char* name,
                       long exp, int apprentice)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        snprintf(
            msg, sizeof(msg),
            "select re_exp from recruit_exp where re_master=%s and re_id=%s",
            master, id);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_update_recruit: query db fail! %s", g_db->error());
            return false;
        }

        int n = result.size();
        if (n) {
            snprintf(msg, sizeof(msg),
                     "update recruit_exp set re_name='%s', re_exp=%ld, "
                     "re_apprentice=%d where re_master=%s and re_id=%s",
                     name, exp, apprentice, master, id);
        }
        else {
            snprintf(msg, sizeof(msg),
                     "insert into recruit_exp(re_master, re_id, re_name, "
                     "re_exp, re_apprentice) values(%s, %s, '%s', %ld, %d)",
                     master, id, name, exp, apprentice);
        }

        ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_update_recruit: execute fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_update_recruit: unknown exception");
        return false;
    }
}

bool db_query_relative(const char* me, const char* target, long& out_point,
                       int& out_relative)
{
    if (!g_db) return false;

    try {
        char msg[256];
        DbResultSet result;
        snprintf(msg, sizeof(msg),
                 "select cc_point, cc_relative from relative where cc_id=%s "
                 "and cc_other=%s",
                 me, target);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_query_relative: query db fail! %s", g_db->error());
            return false;
        }

        if (result.empty() || result.rows[0].columns.size() < 2) {
            return false;
        }

        out_point    = atol(result.rows[0][0].c_str());
        out_relative = atoi(result.rows[0][1].c_str());
        return true;
    } catch (...) {
        LOG_ERROR("db_query_relative: unknown exception");
        return false;
    }
}

bool db_update_relative(const char* me, const char* target, long pt, int flag)
{
    if (!g_db) return false;

    try {
        char msg[512];
        DbResultSet result;

        snprintf(msg, sizeof(msg),
                 "select cc_point from relative where cc_id=%s and cc_other=%s",
                 me, target);
        int ret_code = g_db->query(msg, result);
        if (ret_code != 0) {
            LOG_ERROR("db_update_relative: query db fail! %s", g_db->error());
            return false;
        }

        int n = result.size();
        if (flag == -1) {  // 不修改关系
            if (n) {
                snprintf(msg, sizeof(msg),
                         "update relative set cc_point=%ld where cc_id=%s and "
                         "cc_other=%s",
                         pt, me, target);
            }
            else {
                snprintf(msg, sizeof(msg),
                         "insert into relative(cc_id, cc_other, cc_point, "
                         "cc_relative) values(%s, %s, %ld, 0)",
                         me, target, pt);
            }
        }
        else {
            if (n) {
                snprintf(msg, sizeof(msg),
                         "update relative set cc_point=%ld, cc_relative=%d "
                         "where cc_id=%s and cc_other=%s",
                         pt, flag, me, target);
            }
            else {
                snprintf(msg, sizeof(msg),
                         "insert into relative(cc_id, cc_other, cc_point, "
                         "cc_relative) values(%s, %s, %ld, %d)",
                         me, target, pt, flag);
            }
        }

        ret_code = g_db->execute(msg);
        if (ret_code != 0) {
            LOG_ERROR("db_update_relative: execute fail! %s", g_db->error());
            return false;
        }
        return true;
    } catch (...) {
        LOG_ERROR("db_update_relative: unknown exception");
        return false;
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// register_char(name, passwd, email, gender, str, int, con, dex) -> id, err
// 注册新账号：成功返回新账号 id 与空错误串；失败返回 0 与错误信息。
int l_register_char(lua_State* L)
{
    const char* name   = luaL_checkstring(L, 1);
    const char* passwd = luaL_checkstring(L, 2);
    const char* email  = luaL_checkstring(L, 3);
    const char* gender = luaL_checkstring(L, 4);
    int str            = (int)luaL_checkinteger(L, 5);
    int intel          = (int)luaL_checkinteger(L, 6);
    int con            = (int)luaL_checkinteger(L, 7);
    int dex            = (int)luaL_checkinteger(L, 8);

    char err[256];
    long id = db_register_account(name, passwd, email, gender, str, intel, con,
                                  dex, err, sizeof(err));
    lua_pushinteger(L, id);
    lua_pushstring(L, err);
    return 2;
}

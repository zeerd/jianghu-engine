// test_credential_sql.h — 落库 SQL 的凭据断言辅助
//
// 改密/注册生成的 SQL 里只有盐与哈希（明文列恒为空），"SQL 里出现了
// cc_passwd_hash"证明不了落的是哪个口令。这里取出字面量按指定口令重算，
// 把断言钉在「规范化后的口令 → 这份盐与哈希」上。
#ifndef SERVERTEST_TEST_CREDENTIAL_SQL_H
#define SERVERTEST_TEST_CREDENTIAL_SQL_H

#include <cstdint>
#include <string>
#include <vector>

#include "Crypto.h"

namespace credential_sql {

// 定位 key='value' 字面量（UPDATE 形态：set cc_passwd_salt='...'）
inline bool extract(const std::string& sql, const std::string& key,
                    std::string& out)
{
    const std::string needle = key + "='";
    size_t pos               = sql.find(needle);
    if (pos == std::string::npos) return false;
    size_t begin = pos + needle.size();
    size_t end   = sql.find('\'', begin);
    if (end == std::string::npos) return false;
    out = sql.substr(begin, end - begin);
    return true;
}

inline std::string trim_copy(const std::string& s)
{
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

// 取 INSERT 中指定列的字面量值（列清单与值清单按位置配对）。
// 值按逗号切分——注册用例的取值（姓名/邮箱/Base64/数值）不含逗号。
inline bool insert_value(const std::string& sql, const std::string& column,
                         std::string& out)
{
    size_t cols_begin = sql.find('(');
    size_t cols_end   = sql.find(')', cols_begin);
    size_t vals_begin = sql.find('(', cols_end);
    size_t vals_end   = sql.rfind(')');
    if (cols_begin == std::string::npos || cols_end == std::string::npos
        || vals_begin == std::string::npos || vals_end == std::string::npos)
        return false;

    std::vector<std::string> cols, vals;
    auto split = [](const std::string& body, std::vector<std::string>& out_v) {
        size_t start = 0;
        while (true) {
            size_t comma = body.find(',', start);
            if (comma == std::string::npos) {
                out_v.push_back(body.substr(start));
                return;
            }
            out_v.push_back(body.substr(start, comma - start));
            start = comma + 1;
        }
    };
    split(sql.substr(cols_begin + 1, cols_end - cols_begin - 1), cols);
    split(sql.substr(vals_begin + 1, vals_end - vals_begin - 1), vals);
    if (cols.size() != vals.size()) return false;

    for (size_t i = 0; i < cols.size(); i++) {
        if (trim_copy(cols[i]) != column) continue;
        std::string v = trim_copy(vals[i]);
        if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'')
            v = v.substr(1, v.size() - 2);
        out = v;
        return true;
    }
    return false;
}

// 取出盐与哈希字面量（UPDATE 用 key='v' 定位，INSERT 按列序配对）
inline bool credential_literals(const std::string& sql,
                                const std::string& key_prefix,
                                std::string& salt, std::string& hash)
{
    const std::string salt_key = key_prefix + "_salt";
    const std::string hash_key = key_prefix + "_hash";
    bool ok_s
        = extract(sql, salt_key, salt) || insert_value(sql, salt_key, salt);
    bool ok_h
        = extract(sql, hash_key, hash) || insert_value(sql, hash_key, hash);
    return ok_s && ok_h;
}

// SQL 里 key_prefix（"cc_passwd" / "cc_verifypass"）的盐与哈希是否正是
// pass 按生产迭代次数派生的结果
inline bool matches(const std::string& sql, const std::string& key_prefix,
                    const std::string& pass)
{
    std::string salt, hash;
    if (!credential_literals(sql, key_prefix, salt, hash)) return false;

    std::vector<uint8_t> salt_bytes;
    if (!base64_decode(salt, salt_bytes)) return false;

    uint8_t derived[CRYPTO_HASH_LEN];
    if (!pbkdf2_hmac_sha256(pass.c_str(), salt_bytes.data(), salt_bytes.size(),
                            PBKDF2_ITERATIONS, derived, sizeof(derived)))
        return false;
    return base64_encode(derived, sizeof(derived)) == hash;
}

// 取出明文列的字面量（UPDATE 形态恒为 ''；INSERT 形态按列序取）
inline std::string plaintext_column(const std::string& sql,
                                    const std::string& column)
{
    std::string out;
    if (extract(sql, column, out)) return out;
    if (insert_value(sql, column, out)) return out;
    return "<missing>";
}

}  // namespace credential_sql

#endif  // SERVERTEST_TEST_CREDENTIAL_SQL_H

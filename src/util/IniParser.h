#ifndef INI_PARSER_H
#define INI_PARSER_H

/**
 * @file IniParser.h
 * @brief 轻量级 INI 配置文件解析器（header-only），支持 [section] 分组。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>

// 去除首尾空白
static inline std::string ini_trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// section -> { key -> value }
extern std::map<std::string, std::map<std::string, std::string>> g_ini_data;

// 加载 INI 文件。返回 0 成功，-1 失败（文件不存在等，此时使用默认值）
static inline int ini_load(const char* path)
{
    g_ini_data.clear();

    FILE* fp = fopen(path, "rt");
    if (!fp) return -1;

    std::string current_section = "";
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        std::string s = ini_trim(line);
        if (s.empty()) continue;

        // 注释行
        if (s[0] == ';' || s[0] == '#') continue;

        // [section]
        if (s[0] == '[') {
            size_t pos = s.find(']');
            if (pos != std::string::npos)
                current_section = ini_trim(s.substr(1, pos - 1));
            continue;
        }

        // key = value
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;

        std::string key = ini_trim(s.substr(0, eq));
        std::string val = ini_trim(s.substr(eq + 1));

        // 去除 value 中的行内注释（; 或 #）
        size_t comment = val.find(" ;");
        if (comment == std::string::npos) comment = val.find("\t;");
        if (comment == std::string::npos) comment = val.find(" #");
        if (comment == std::string::npos) comment = val.find("\t#");
        if (comment != std::string::npos)
            val = ini_trim(val.substr(0, comment));

        // 去除引号包裹
        if (val.size() >= 2
            && ((val[0] == '"' && val[val.size() - 1] == '"')
                || (val[0] == '\'' && val[val.size() - 1] == '\''))) {
            val = val.substr(1, val.size() - 2);
        }

        g_ini_data[current_section][key] = val;
    }

    fclose(fp);
    return 0;
}

// 获取字符串值，section 为 NULL 或空字符串时在全局（无 section）中查找
static inline const char* ini_get(const char* section, const char* key,
                                  const char* def)
{
    if (!key) return def;

    std::string sec = section ? section : "";
    std::string k   = key;

    auto it = g_ini_data.find(sec);
    if (it == g_ini_data.end()) return def;

    auto kv = it->second.find(k);
    if (kv == it->second.end()) return def;

    return kv->second.c_str();
}

// 获取整数值
static inline int ini_get_int(const char* section, const char* key, int def)
{
    const char* v = ini_get(section, key, NULL);
    if (!v) return def;
    return atoi(v);
}

// 获取布尔值（true/1/yes/on 为真）
static inline int ini_get_bool(const char* section, const char* key, int def)
{
    const char* v = ini_get(section, key, NULL);
    if (!v) return def;
    if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0
        || strcmp(v, "on") == 0)
        return 1;
    if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0 || strcmp(v, "no") == 0
        || strcmp(v, "off") == 0)
        return 0;
    return atoi(v);
}

#endif  // INI_PARSER_H

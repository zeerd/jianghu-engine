/** @file StringUtil.cpp @brief 通用字符串工具函数  */

#include "StringUtil.h"
#include "../object/Global.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//---------------------------------------------------------
// 分析指令：将命令和参数分离
//---------------------------------------------------------
void analyse_string(char* Comm, char* arg, int len)
{
    int p = 0, a = 0;

    while (Comm[p] && Comm[p] != ' ') p++;

    if (Comm[p] == ' ') {
        Comm[p] = 0;
        p++;

        while (Comm[p] && a < len - 1) {
            arg[a++] = Comm[p++];
        }
    }

    arg[a] = 0;
}

//---------------------------------------------------------
// 解析 key=value 参数
//---------------------------------------------------------
char* get_value(const char* cmd_str, const char* keyword, char* pValue,
                int nValueLen)
{
    char szKeyWord[80];
    char* pKey;
    int nCopyLen;

    pValue[0] = 0;

    snprintf(szKeyWord, 80, "&%s=", keyword);
    pKey = (char*)strstr(cmd_str, szKeyWord);

    if (!pKey) return pValue;

    pKey += strlen(szKeyWord);

    nCopyLen = strchr(pKey, '&') - pKey;

    if (nCopyLen <= 0) return pValue;

    if (nCopyLen > nValueLen - 1) nCopyLen = nValueLen - 1;

    strncpy(pValue, pKey, nCopyLen)[nCopyLen] = 0;

    return pValue;
}

//---------------------------------------------------------
// 格式化 snprintf（标准函数封装，返回 char*）
//---------------------------------------------------------
char* snprintf(char* buf, int len, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, len, fmt, ap);
    va_end(ap);
    return buf;
}

//---------------------------------------------------------
// 译码
//---------------------------------------------------------
char* decode_str(char* msg)
{
    int p = 0, t = 0;
    const char* decode = "\15L\26A\18N\12W\25O\33O\6D";

    int len = strlen(decode);
    while (msg[p]) {
        if (msg[p] != ' ' && msg[p] != '\n' && msg[p] != '\t') {
            msg[p] ^= decode[t];
            if (msg[p] == '\t') msg[p] = '\t' ^ decode[t];
            if (msg[p] == '\n') msg[p] = '\n' ^ decode[t];
            if (msg[p] == ' ') msg[p] = ' ' ^ decode[t];
            if (msg[p] == 0) msg[p] = 0 ^ decode[t];
        }

        p++;
        t++;
        if (t >= len) t = 0;
    }
    return msg;
}

//---------------------------------------------------------
// 校验字符串合法性
//---------------------------------------------------------
int valid_string(const char* msg)
{
    int p = 0;
    while (msg[p]) {
        if (msg[p] == '\"') return notify_fail("不能含有引号!");
        p++;
    }

    return 1;
}

int cc_strcmp(const char* str1, const char* str2)
{
    if (str1 == NULL) {
        return -1;
    }
    else if (str2 == NULL) {
        return 1;
    }
    else {
        return strcmp(str1, str2);
    }
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// valid_string(msg) -> number - 校验文本不含引号，合法返回1，否则 notify_fail
// 返回0
int l_valid_string(lua_State* L)
{
    const char* msg = luaL_checkstring(L, 1);
    lua_pushinteger(L, valid_string(msg));
    return 1;
}

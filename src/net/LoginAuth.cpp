/**
 * @file LoginAuth.cpp
 * @brief 认证/安全/工具函数 */

#include "Global.h"
#include "User.h"
#include "Char.h"
#include "Channel.h"
#include "CObject.h"
#include "LoginAuth.h"
#include "Sql.h"

int ban_me(CChar* me)
{
    if (g_channel.banlist.query(get_ip(me))) return 1;

    char MainIP[80];
    strncpy(MainIP, me->querystr_temp("client/ip_address"), 20)[19] = 0;
    if (g_channel.banlist.query(MainIP)) return 1;

    strncpy(MainIP, get_ip(me), 80)[79] = 0;

    char* ptr = strstr(MainIP, ".");
    if (!ptr) return 0;
    ptr = strstr(ptr + 1, ".");
    if (!ptr) return 0;
    ptr = strstr(ptr + 1, ".");
    if (!ptr) return 0;
    ptr = strstr(ptr + 1, ".");
    if (!ptr) return 0;

    *ptr = 0;
    if (g_channel.banlist.query(MainIP)) return 1;

    return 0;
}

char* make_ip(char* main_ip, char* local_ip)
{
    static char myIP[40];
    if (strcmp(main_ip, local_ip)) {
        snprintf(myIP, 40, "M:%s L:%s", main_ip, local_ip);
    }
    else {
        snprintf(myIP, 40, "%s", main_ip);
    }
    return myIP;
}

char* get_ip(CChar* me)
{
    char MainIP[20], LocalIP[20];

    strncpy(MainIP, me->querystr_temp("client/ip_address"), 20)[19] = 0;
    strncpy(LocalIP, me->querystr_temp("client/ip_local"), 20)[19]  = 0;
    return make_ip(MainIP, LocalIP);
}

int is_wizard(CChar* me) { return me->query("grant") >= CChar::GRANT_WIZARD; }

int is_user(CContainer* me)
{
    if (!me) return 0;
    return me->query_attr(CChar::IS_USER);
}

int verify_password(const char* id, const char* pass)
{
    std::string tmp = truncated_copy(pass, 79);
    for (char& c : tmp) {
        if (c == '\'') c = ' ';
    }

    return db_verify_password(id, tmp.c_str()) ? 1 : 0;
}

int change_verify_password(const char* id, const char* pass)
{
    std::string tmp = truncated_copy(pass, 511);
    for (char& c : tmp) {
        if (c == '\'') c = ' ';
    }

    return db_change_verify_password(id, tmp.c_str()) ? 1 : 0;
}

int change_password(const char* id, const char* pass)
{
    return db_change_password(id, pass) ? 1 : 0;
}

int set_invalid_account(const char* id, int freeze)
{
    return db_set_freeze(id, freeze) ? 1 : 0;
}

int query_invalid_account(const char* id) { return db_query_freeze(id); }

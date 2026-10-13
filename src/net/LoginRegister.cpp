/**
 * @file LoginRegister.cpp
 * @brief 注册/名字校验 */

#include "Global.h"
#include "User.h"
#include "Char.h"
#include "LoginRegister.h"
#include "LoginAuth.h"
#include "Sql.h"
#include "../lua/LuaEngine.h"

extern int g_current_account;

void setup_char(CChar* me);

// 调用 Lua 的 login.check_name(name) 进行名字校验。
// 返回 "" 表示合法，非空为错误信息。
// Lua 不可用或函数不存在时回退到最小 C++ 实现（仅检查长度）。
static const char* call_lua_check_name(const char* name)
{
    if (!g_LuaEngine) return "";
    lua_State* L = g_LuaEngine->get_state();
    if (!L) return "";

    // require("data.login")
    lua_getglobal(L, "require");
    lua_pushstring(L, "data.login");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_istable(L, -1)) {
        lua_settop(L, 0);
        return "";
    }

    // login.check_name(name)
    lua_getfield(L, -1, "check_name");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return "";
    }

    lua_pushstring(L, name);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return "";
    }

    const char* result = lua_tostring(L, -1);
    // 注意：不能 pop，因为 result 指向 Lua 栈上的字符串。
    // 下次调用前 lua_settop 会清理。
    return result ? result : "";
}

// 名字校验入口。
// 所有 C++ 调用方（MakeBody、CmdUser.h、Sql.cpp 等）统一走此函数，
// 内部优先调用 Lua（login.check_name），Lua 不可用时回退长度检查。
char* check_legal_name(const char* name)
{
    if (strlen(name) < 2 || strlen(name) > 12) {
        return "对不起，你的中文名字必须是 1 到 6 个中文字。";
    }

    const char* err = call_lua_check_name(name);
    return (char*)err;
}
char* make_body(char* name, char* passwd, char* gender, int str, int con,
                int intel, int dex)
{
    static char msg[255];
    char id[20];
    CUser user;

    if (strlen(name) < 2 || strlen(name) > 10) return "姓名的长度非法。";

    strcpy(msg, check_legal_name(name));

    if (msg[0]) return msg;

    if (str + con + intel + dex != 84) return "天赋属性非法。";

    snprintf(id, 20, "%d", ++g_current_account);

    user.set_name(name, id);
    user.set("gender", gender);
    user.set("age", 14);

    user.set("str", str);
    user.set("con", con);
    user.set("int", intel);
    user.set("dex", dex);

    setup_char(&user);
    user.set("title", "普通百姓");
    user.set("birthday", g_current_time);
    user.set("combat_exp", 1000);
    user.set("level", 1);

    user.set("start_city", g_start_room);

    if (!user.save_to_server()) return "档案制作失败！";

    snprintf(msg, 255,
             "&C=%d&M=注册成功，用户帐号：%d，姓名：%s，请登录游戏！&\t",
             NOTIFY, g_current_account, name);
    return msg;
}

int check_valid_char(char* arg)
{
    bool bHefa;
    unsigned char zifu;
    int i = 0;

    while ((zifu = (unsigned char)arg[i++])) {
        bHefa = false;

        if ((zifu >= 'A' && zifu <= 'Z') || (zifu >= 'a' && zifu <= 'z'))
            bHefa = true;

        if (zifu >= 160 && zifu < 255) {
            i++;
            bHefa = true;
        }

        if (!bHefa) return 0;
    }
    return 1;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// check_legal_name(name) -> string - 校验名字，非法返回错误信息，合法返回 ""
// C++ check_legal_name 内部已优先调用 Lua（login.check_name），此处直接转发
int l_check_legal_name(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* s    = check_legal_name(name);
    lua_pushstring(L, s ? s : "");
    return 1;
}

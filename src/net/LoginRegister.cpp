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
// 新角色初始档案：
//
// age 必须在 setup_char 之前给定：缺省会被 setup_human_base_attrs 随机成
// 15~44，setup_human_max_hp 于是按随机 age、且 level 此刻尚未写入（按 0）
// 算出虚高的 max_hp，setup_char 随后又把 hp 按这个 max_hp 补满——角色会
// 带着 hp > 实际上限上线，要等心跳 heal_up 才被压回。
// level 是新角色的起手值（起手经验为 0，保持缺省不写）。
void init_new_char(CUser& me, const char* name, const char* id,
                   const char* gender, int str, int con, int intel, int dex)
{
    me.set_name(name, id);
    me.set("gender", gender);
    me.set("age", NEW_CHAR_AGE);

    me.set("str", str);
    me.set("con", con);
    me.set("int", intel);
    me.set("dex", dex);

    setup_char(&me);

    me.set("title", "普通百姓");
    me.set("birthday", g_current_time);
    me.set("level", NEW_CHAR_LEVEL);

    me.set("start_city", g_start_room);
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

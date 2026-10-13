#include "Global.h"
#include "Random.h"

//---------------------------------------------------------
// 随机数
//---------------------------------------------------------
int32_t CUtils::random(int32_t seed)
{
    if (seed <= 0) return 0;
    static double fRandMax = (double)RAND_MAX + 1.0;
    double f               = (double)rand() / fRandMax;
    f *= seed;
    return (int32_t)f;
}

double CUtils::urandom(double seed)
{
    if (seed <= 0) return 0;
    static double fRandMax = (double)RAND_MAX + 1.0;
    double f               = (double)rand() / fRandMax;
    f *= seed;
    return f;
}

//---------------------------------------------------------
// [LuaBind] auto-managed bindings
//---------------------------------------------------------
#include "LuaBindIncludes.h"  // NOLINT (needed for scattered binds)

// g_utils.random(n) -> number
int l_random(lua_State* L)
{
    int n = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, g_utils.random(n));
    return 1;
}

// xrandom(v) - 宝物随机（保留为 C++ 全局函数，因 create() 时 Lua 模块尚未加载）
int l_xrandom(lua_State* L)
{
    int v = (int)luaL_checkinteger(L, 1);
    // 纯数学实现，不依赖任何 C++ 类
    int c   = g_utils.random(20);
    float b = (float)(g_utils.random(v)) / 5.0f;
    int result;
    if (c < 1)
        result = (int)((float)v * 4.0f / 5.0f + b);
    else if (c < 3)
        result = (int)((float)v * 3.0f / 5.0f + b);
    else if (c < 6)
        result = (int)((float)v * 2.0f / 5.0f + b);
    else if (c < 12)
        result = (int)((float)v * 1.0f / 5.0f + b);
    else
        result = (int)b;
    lua_pushinteger(L, result);
    return 1;
}

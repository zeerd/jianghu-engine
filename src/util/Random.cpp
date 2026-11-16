#include "Global.h"
#include "Random.h"

//---------------------------------------------------------
// 随机数
//---------------------------------------------------------
mapping_value_t CUtils::random(mapping_value_t seed)
{
    if (seed <= 0) return 0;
    static double fRandMax = (double)RAND_MAX + 1.0;
    double f               = (double)rand() / fRandMax;
    f *= (double)seed;
    return static_cast<mapping_value_t>(f);
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

// g_utils.random(n) -> number：返回 [0, n-1]，seed <= 0 时返回 0。
// 已废弃（脚本侧）：Lua 标准库 math.random 返回 [1, n]，两套语义相反——按
// math.random 的习惯写 t[random(n)] 会取到 t[0] = nil，写 random(mp / 2) 会
// 因实参无整数表示被 luaL_checkinteger 拒收。本绑定仅为存量数据集保留；
// lua-default / lua-sample 与新脚本一律改用 math.random。
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
    int c   = static_cast<int>(g_utils.random(20));
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

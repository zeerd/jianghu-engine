#include <windows.h>

#include <string>

#include "Global.h"
#include "Char.h"
#include "LuaHook.h"

namespace {

// 角色类称谓询问：phase 决定算法，a 为对象、b 为对方（可空）。
// 结果写入调用方持有的独立静态缓冲，避免同一表达式内混用不同称谓时相互覆写。
void rank_fill(const char* phase, CChar* a, CChar* b, std::string& out)
{
    out.clear();
    CLuaHookCall hook("rank_rule");
    hook.add_str(phase).add_object(a).add_object(b);
    if (!hook.call()) return;
    const char* s = hook.result_str();
    if (s) out = s;
}

// 外貌询问：a 为性别字符串、b 为容貌值。
void waimao_fill(const std::string& gender, int per, std::string& out)
{
    out.clear();
    CLuaHookCall hook("rank_rule");
    hook.add_str("waimao").add_str(gender.c_str()).add_int(per);
    if (!hook.call()) return;
    const char* s = hook.result_str();
    if (s) out = s;
}

}  // namespace

const char* query_rank(CChar* ob)
{
    static std::string buf;
    rank_fill("rank", ob, nullptr, buf);
    return buf.c_str();
}

const char* query_respect(CChar* ob)
{
    static std::string buf;
    rank_fill("respect", ob, nullptr, buf);
    return buf.c_str();
}

const char* query_rude(CChar* ob)
{
    static std::string buf;
    rank_fill("rude", ob, nullptr, buf);
    return buf.c_str();
}

const char* query_self(CChar* ob)
{
    static std::string buf;
    rank_fill("self", ob, nullptr, buf);
    return buf.c_str();
}

const char* query_self_rude(CChar* ob)
{
    static std::string buf;
    rank_fill("self_rude", ob, nullptr, buf);
    return buf.c_str();
}

const char* query_close(CChar* me, CChar* ob)
{
    static std::string buf;
    rank_fill("close", me, ob, buf);
    return buf.c_str();
}

const char* query_self_close(CChar* me, CChar* ob)
{
    static std::string buf;
    rank_fill("self_close", me, ob, buf);
    return buf.c_str();
}

const char* query_waimao(string gender, int per)
{
    static std::string buf;
    waimao_fill(gender, per, buf);
    return buf.c_str();
}

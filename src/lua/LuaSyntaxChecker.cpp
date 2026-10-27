/** @file LuaSyntaxChecker.cpp
 *  @brief --check-lua 静态校验实现。
 *
 *  四道防线，各自对应一次真实线上故障（都是先炸后发现 → 逐道前移到启动前）：
 *
 *  1. 语法          luaL_loadbuffer：括号/引号不匹配、关键字拼错。
 *  2. require 闭包  字面量 require("a.b") 指向不存在的模块（历史事故：
 *                   lua-default/skill/high/animal/*.lua 引用不存在的
 *                   skill.skill_utils，语法全绿、加载即炸）。
 *  3. 未绑定全局    data/card_core.lua 调 get_db_type()，引擎从未注册 →
 *                   登录期 login.on_login 抛
 *                   "attempt to call a nil value (global 'get_db_type')"。
 *  4. 未绑定方法    self:set_verbs(...) / self:Ready_PK() —— 对象元表里只有
 *                   set_beast_verbs / ready_pk，名字或大小写不符即取到 nil。
 *                   同型：Pay_Player 对 pay_player。
 *
 *  第 3、4 道必须拿到"引擎到底绑了什么"才有判断依据，故本文件自持调用
 *  lua_bind_init()（幂等），不依赖调用方是否已初始化——历史上 check-lua 只跑
 *  CLuaEngine::init()，头注释宣称"完整绑定环境"却从未绑定，等于没有依据。
 *
 *  存量违规经基线文件豁免（只拦新增），避免一次性卡死验证链路。
 *
 *  已知盲区（工具是底线不是标准）：
 *   1. 动态 require（require(name)、require("a" .. b)）只记 NOTE 不判失败；
 *   2. locals 是全文件平面集合：任一处声明过同名 local，这个名字在本文件内
 *      就不再检查——局部与全局同名时会漏检（宁可漏、不可误报的取舍）；
 *   3. 方法消解只看"全语料有没有人定义过这个名字"，不校验目标对象上真的
 *      挂得到，跨对象同名会被放行。
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "LuaSyntaxChecker.h"
#include "LuaEngine.h"
#include "LuaBind.h"
#include "IniParser.h"

// --check-lua 参数解析辅助：查找下一个参数（跳过空白）
static const char* skip_arg(const char* p)
{
    while (*p && (*p == ' ' || *p == '\t')) ++p;
    return p;
}

// 提取一个参数值（到下一个空白或字符串结尾），并前移 p
static std::string extract_arg(const char*& p)
{
    if (*p == '"') {
        ++p;
        const char* start = p;
        while (*p && *p != '"') ++p;
        std::string out(start, p - start);
        if (*p == '"') ++p;
        return out;
    }
    const char* start = p;
    while (*p && *p != ' ' && *p != '\t') ++p;
    return std::string(start, p - start);
}

// 递归收集目录下所有 .lua 文件
static void collect_lua_files(const std::string& dir,
                              std::vector<std::string>& out)
{
    std::string pattern = dir + "\\*.lua";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                out.push_back(dir + "\\" + fd.cFileName);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // 递归子目录
    std::string subPattern = dir + "\\*";
    hFind                  = FindFirstFileA(subPattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (strcmp(fd.cFileName, ".") != 0
                    && strcmp(fd.cFileName, "..") != 0) {
                    collect_lua_files(dir + "\\" + fd.cFileName, out);
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
}

namespace {

// ======================== 词法 ========================

enum TokKind { LT_NAME, LT_STR, LT_NUM, LT_OP, LT_KW };

struct LexTok {
    TokKind kind;
    std::string text;
    int line;
};

// 源码经 load_file_as_gbk 统一为 GBK。GBK 双字节的尾字节可落在 0x40-0x7E，
// 与引号/括号同域；不按双字节跳过就会把中文字节当成字符串边界，截断 token 流。
inline bool is_gbk_lead(unsigned char c) { return c >= 0x81; }

bool is_lua_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_lua_ident(char c)
{
    return is_lua_ident_start(c) || (c >= '0' && c <= '9');
}

// 若 s[i] 处是长括号开括号（[[ 或 [=[ …），经 open_len 返回开括号长度
// （2 + 等号数），并返回对应闭括号串（"]]"、"]=]" 等）；否则返回空串。
std::string long_bracket(const std::string& s, size_t i, size_t& open_len)
{
    open_len = 0;
    if (i >= s.size() || s[i] != '[') return "";
    size_t j = i + 1;
    int eq   = 0;
    while (j < s.size() && s[j] == '=') {
        ++eq;
        ++j;
    }
    if (j >= s.size() || s[j] != '[') return "";
    open_len = 2 + eq;
    return std::string("]") + std::string(eq, '=') + std::string("]");
}

bool is_lua_keyword(const std::string& word)
{
    static const char* kKeywords[] = {
        "and",      "break",  "do",   "else", "elseif", "end",  "false", "for",
        "function", "goto",   "if",   "in",   "local",  "nil",  "not",   "or",
        "repeat",   "return", "then", "true", "until",  "while"};
    return std::find(std::begin(kKeywords), std::end(kKeywords), word)
           != std::end(kKeywords);
}

// 词法扫描：跳过注释，产出带行号的 token；短/长字符串保留引号内原始内容
// （require("a.b") 的模块名直接取自 token，不做转义解码，与历史行为一致）。
void lex_lua(const std::string& src, std::vector<LexTok>& out)
{
    const size_t n = src.size();
    size_t i       = 0;
    int line       = 1;
    while (i < n) {
        const unsigned char c = (unsigned char)src[i];

        if (c == '\n') {
            ++line;
            ++i;
            continue;
        }
        if (c == '\r' || c == ' ' || c == '\t') {
            ++i;
            continue;
        }
        if (is_gbk_lead(c)) {
            i += 2;  // GBK 双字节整体跳过，尾字节不参与结构判定
            continue;
        }

        // 注释：-- … 或 --[[ … ]]
        if (c == '-' && i + 1 < n && src[i + 1] == '-') {
            size_t open_len   = 0;
            std::string close = long_bracket(src, i + 2, open_len);
            if (!close.empty()) {
                size_t e = src.find(close, i + 2 + open_len);
                if (e == std::string::npos) return;
                const std::string seg = src.substr(i, e + close.size() - i);
                line += (int)std::count(seg.begin(), seg.end(), '\n');
                i = e + close.size();
            }
            else {
                size_t e = src.find('\n', i + 2);
                if (e == std::string::npos) return;
                i = e;
            }
            continue;
        }

        // 短字符串（内部按 GBK 双字节跳，尾字节可能是引号）
        if (c == '"' || c == '\'') {
            const int start_line = line;
            size_t j             = i + 1;
            while (j < n && src[j] != c) {
                const unsigned char sc = (unsigned char)src[j];
                if (is_gbk_lead(sc)) {
                    if (j + 1 < n && src[j + 1] == '\n') ++line;
                    j += 2;
                    continue;
                }
                if (sc == '\\') {
                    ++j;
                    if (j < n && src[j] == '\n') ++line;
                    ++j;
                    continue;
                }
                if (sc == '\n') ++line;
                ++j;
            }
            out.push_back({LT_STR, src.substr(i + 1, j - (i + 1)), start_line});
            i = (j < n) ? j + 1 : n;
            continue;
        }

        // 长字符串
        if (c == '[') {
            size_t open_len   = 0;
            std::string close = long_bracket(src, i, open_len);
            if (!close.empty()) {
                size_t e = src.find(close, i + open_len);
                if (e == std::string::npos) return;
                const std::string seg = src.substr(i, e + open_len - i);
                out.push_back({LT_STR,
                               src.substr(i + open_len, e - (i + open_len)),
                               line});
                line += (int)std::count(seg.begin(), seg.end(), '\n');
                i = e + close.size();
                continue;
            }
        }

        // 标识符 / 关键字
        if (is_lua_ident_start((char)c)) {
            size_t j = i;
            while (j < n && is_lua_ident(src[j])) ++j;
            const std::string word = src.substr(i, j - i);
            out.push_back({is_lua_keyword(word) ? LT_KW : LT_NAME, word, line});
            i = j;
            continue;
        }

        // 数字（含 0x 十六进制与指数，否则 0x41 会被拆成 NUM "0" +
        // NAME "x41"，把 x41 误报成未绑定全局）
        if ((c >= '0' && c <= '9')
            || (c == '.' && i + 1 < n && src[i + 1] >= '0'
                && src[i + 1] <= '9')) {
            const auto is_hex_digit = [](unsigned char h) {
                return (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f')
                       || (h >= 'A' && h <= 'F');
            };
            const bool is_hex = (c == '0' && i + 1 < n
                                 && (src[i + 1] == 'x' || src[i + 1] == 'X'));
            size_t j          = is_hex ? i + 2 : i;
            if (is_hex) {
                while (j < n) {
                    const unsigned char h = (unsigned char)src[j];
                    if (is_hex_digit(h)) {
                        ++j;
                    }
                    else if (src[j] == '.' && j + 1 < n
                             && is_hex_digit((unsigned char)src[j + 1])
                             && src[j + 1] != '.') {
                        ++j;
                    }
                    else {
                        break;
                    }
                }
                if (j < n && (src[j] == 'p' || src[j] == 'P')) {
                    ++j;
                    if (j < n && (src[j] == '+' || src[j] == '-')) ++j;
                    while (j < n && src[j] >= '0' && src[j] <= '9') ++j;
                }
            }
            else {
                while (j < n) {
                    const unsigned char h = (unsigned char)src[j];
                    if (h >= '0' && h <= '9') {
                        ++j;
                    }
                    else if (src[j] == '.' && j + 1 < n && src[j + 1] != '.') {
                        ++j;  // 1.5，但不吃 1..5 的连接符
                    }
                    else {
                        break;
                    }
                }
                if (j < n && (src[j] == 'e' || src[j] == 'E')) {
                    const size_t save = j;
                    ++j;
                    if (j < n && (src[j] == '+' || src[j] == '-')) ++j;
                    if (j < n && src[j] >= '0' && src[j] <= '9') {
                        while (j < n && src[j] >= '0' && src[j] <= '9') ++j;
                    }
                    else {
                        j = save;  // 1e 不是指数，e 归下一 token
                    }
                }
            }
            out.push_back({LT_NUM, src.substr(i, j - i), line});
            i = j;
            continue;
        }

        // 两字符运算符
        if (i + 1 < n) {
            const std::string two = src.substr(i, 2);
            if (two == ".." || two == "==" || two == "~=" || two == "<="
                || two == ">=" || two == "::" || two == "//") {
                out.push_back({LT_OP, two, line});
                i += 2;
                continue;
            }
        }
        out.push_back({LT_OP, std::string(1, (char)c), line});
        ++i;
    }
}

// ======================== 基线键 ========================

// 控制字符转义，使基线文件保持单行可读；GBK 高位字节原样透传（数据集本身即
// GBK）
std::string key_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = (unsigned char)s[i];
        if (c == '\\') {
            out += "\\\\";
        }
        else if (c == '\n') {
            out += "\\n";
        }
        else if (c == '\r') {
            out += "\\r";
        }
        else if (c < 0x20 || c == 0x7F) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        }
        else {
            out += (char)c;
        }
    }
    return out;
}

std::string make_rel_path(const std::string& path, const char* prefix)
{
    std::string s = path;
    std::replace(s.begin(), s.end(), '\\', '/');
    if (prefix && *prefix) {
        std::string pre = prefix;
        std::replace(pre.begin(), pre.end(), '\\', '/');
        if (pre.empty() || pre.back() != '/') pre += '/';
        if (s.size() > pre.size() && s.compare(0, pre.size(), pre) == 0) {
            s = s.substr(pre.size());
        }
    }
    return s;
}

// ======================== 作用域与定义收集 ========================

// `a, b = ...` 的左值判定：从 i 起是 `NAME [, NAME]* =` 形态时，经 eq 返回
// '=' 的下标并返回序列的 token 数（含逗号），否则返回 0。
// 赋值目标是写入、不是读取，且每一个目标都要登记为定义——只认紧邻 '=' 的
// 最后一个目标，会把 `x, y = unpack(t)` 的 x 当成全局读取报出来。
size_t assign_lhs(const std::vector<LexTok>& toks, size_t i, size_t& eq)
{
    const size_t n = toks.size();
    eq             = 0;
    if (i >= n || toks[i].kind != LT_NAME) return 0;

    size_t m = i;
    while (m < n && toks[m].kind == LT_NAME) {
        ++m;
        if (m < n && toks[m].kind == LT_OP && toks[m].text == ",") {
            ++m;
            continue;
        }
        break;
    }
    if (m == i) return 0;
    if (m < n && toks[m].kind == LT_OP && toks[m].text == "=") {
        eq = m;
        return m - i;
    }
    return 0;
}

// 一个文件的静态分析结果：
//   locals  —— 局部名（local / 函数参数 / for 变量），读到即非全局；
//   defs    —— 本文件建立的全局名，跨文件互认；
//   methods —— 本文件定义的方法名，供方法名检测消解。
struct FileScan {
    std::set<std::string> locals;
    std::set<std::string> defs;
    std::set<std::string> methods;
};

void analyze_scope(const std::vector<LexTok>& toks, FileScan& fs)
{
    const size_t n = toks.size();

    // 语句上下文栈：'{' '(' '[' 'f'(function) 'b'(if/for/while/do)
    // 'r'(repeat)。 用来判定一个 `name = ...`
    // 是表构造器字段（不建立全局）还是语句级赋值
    // （建立全局）。只数花括号深度会把表构造器里嵌的函数体
    // （`{ init = function() x = 1 end }`）的 x 当成字段，漏记定义 →
    // 跨文件读者被误报成未绑定全局；反之不区分字段又会把 `{ a = 1 }` 的 a
    // 当全局放行，两者都不行，所以按块栈取"最内层容器"来判。
    std::vector<char> blocks;
    std::vector<bool> is_field(n, false);
    bool pending_do = false;  // for / while 头部自带的 do 不单独入栈
    for (size_t k = 0; k < n; ++k) {
        const LexTok& t = toks[k];
        if (t.kind == LT_KW) {
            if (t.text == "function") {
                blocks.push_back('f');
            }
            else if (t.text == "if") {
                blocks.push_back('b');
            }
            else if (t.text == "for" || t.text == "while") {
                blocks.push_back('b');
                pending_do = true;
            }
            else if (t.text == "do") {
                if (pending_do)
                    pending_do = false;
                else
                    blocks.push_back('b');
            }
            else if (t.text == "repeat") {
                blocks.push_back('r');
            }
            else if (t.text == "until") {
                if (!blocks.empty() && blocks.back() == 'r') blocks.pop_back();
            }
            else if (t.text == "end") {
                if (!blocks.empty()
                    && (blocks.back() == 'f' || blocks.back() == 'b'))
                    blocks.pop_back();
            }
            continue;
        }
        if (t.kind != LT_OP) continue;
        if (t.text == "{") {
            blocks.push_back('{');
        }
        else if (t.text == "(") {
            blocks.push_back('(');
        }
        else if (t.text == "[") {
            blocks.push_back('[');
        }
        else if (t.text == ")") {
            if (!blocks.empty() && blocks.back() == '(') blocks.pop_back();
        }
        else if (t.text == "]") {
            if (!blocks.empty() && blocks.back() == '[') blocks.pop_back();
        }
        else if (t.text == "}") {
            while (!blocks.empty() && blocks.back() != '{') blocks.pop_back();
            if (!blocks.empty()) blocks.pop_back();
        }
        else if (t.text == "=" && k > 0 && toks[k - 1].kind == LT_NAME) {
            // 从栈顶往下找最近的 { 或 function：先碰到 '{' 才是表字段
            for (size_t s = blocks.size(); s-- > 0;) {
                if (blocks[s] == '{') {
                    is_field[k - 1] = true;
                    break;
                }
                if (blocks[s] == 'f') break;  // 函数体里是语句
            }
        }
    }

    for (size_t k = 0; k < n; ++k) {
        const LexTok& t = toks[k];
        if (t.kind != LT_KW) continue;

        if (t.text == "local") {
            size_t m = k + 1;
            // local function NAME
            if (m < n && toks[m].kind == LT_KW && toks[m].text == "function") {
                if (m + 1 < n && toks[m + 1].kind == LT_NAME)
                    fs.locals.insert(toks[m + 1].text);
                continue;
            }
            // local a, b, c [= ...]
            while (m < n && toks[m].kind == LT_NAME) {
                fs.locals.insert(toks[m].text);
                ++m;
                if (m < n && toks[m].kind == LT_OP && toks[m].text == ",") {
                    ++m;
                }
                else {
                    break;
                }
            }
            continue;
        }

        if (t.text == "function") {
            // 路径：name(.name | :name)*
            std::vector<std::string> path;
            size_t m = k + 1;
            while (m < n && toks[m].kind == LT_NAME) {
                path.push_back(toks[m].text);
                ++m;
                if (m < n && toks[m].kind == LT_OP
                    && (toks[m].text == "." || toks[m].text == ":")) {
                    ++m;
                }
                else {
                    break;
                }
            }
            if (path.size() == 1) {
                if (fs.locals.count(path[0]) == 0) fs.defs.insert(path[0]);
            }
            else if (path.size() > 1) {
                fs.methods.insert(path.back());
            }
            // 形参
            if (m < n && toks[m].kind == LT_OP && toks[m].text == "(") {
                ++m;
                int pd = 1;
                while (m < n && pd > 0) {
                    if (toks[m].kind == LT_OP && toks[m].text == "(") {
                        ++pd;
                    }
                    else if (toks[m].kind == LT_OP && toks[m].text == ")") {
                        --pd;
                    }
                    else if (pd == 1 && toks[m].kind == LT_NAME) {
                        fs.locals.insert(toks[m].text);
                    }
                    ++m;
                }
            }
            continue;
        }

        if (t.text == "for") {
            // for i = ... do / for k, v in ... do —— 只收循环变量，到 `=` 或
            // `in` 就停。`in` 是关键字（LT_KW）不是运算符，按 LT_OP 判永远
            // 匹配不上，会一路把后面的 token 全塞进 locals：既让窗口内的未知
            // 全局读漏检，又把紧随其后的全局定义（`function helper()`、
            // `flag = true`）顶成"本地名"，跨文件读者反被误报。
            size_t m = k + 1;
            while (m < n) {
                const LexTok& h = toks[m];
                if (h.kind == LT_OP && h.text == "=") break;
                if (h.kind == LT_KW && (h.text == "in" || h.text == "do"))
                    break;
                if (h.kind == LT_NAME) fs.locals.insert(h.text);
                ++m;
            }
            continue;
        }
    }

    // 赋值形态：NAME [, NAME]* = ...
    for (size_t k = 0; k < n; ++k) {
        const LexTok& t = toks[k];
        if (t.kind != LT_NAME) continue;
        size_t eq = 0;
        if (assign_lhs(toks, k, eq) == 0) continue;

        const bool member_write
            = k > 0 && toks[k - 1].kind == LT_OP
              && (toks[k - 1].text == "." || toks[k - 1].text == ":");
        const bool rhs_fn = eq + 1 < n && toks[eq + 1].kind == LT_KW
                            && toks[eq + 1].text == "function";

        if (member_write) {
            // _G.name = ... 与 name = ... 等价：漏记定义会让别处的裸读被
            // 误报成未绑定全局（_G.x = 1 不是表字段、也不是本地名）
            const bool via_g = k > 1 && toks[k - 2].kind == LT_NAME
                               && toks[k - 2].text == "_G";
            if (via_g) {
                fs.defs.insert(t.text);
            }
            else if (rhs_fn) {
                // T.name = function(...) —— 挂在别的表上，属于脚本方法；
                // 其余是字段写入，与全局无关
                fs.methods.insert(t.text);
            }
            continue;
        }

        if (fs.locals.count(t.text)) continue;

        if (is_field[k]) {
            // 表构造器内的 { name = function(...) } —— 脚本表方法
            if (rhs_fn) fs.methods.insert(t.text);
            continue;
        }
        // 语句级赋值即建立全局：x = 1、x = function(...) 都算
        // （damage_msg.lua 用 damage_msg = function 定义全局函数，
        //   只归到 methods 会漏掉，把它误报成未绑定全局）
        fs.defs.insert(t.text);
    }

    // rawset(_G, "name", v) —— 与 _G.name = v 同为间接定义全局
    for (size_t k = 0; k + 4 < n; ++k) {
        if (toks[k].kind != LT_NAME || toks[k].text != "rawset") continue;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "(") continue;
        if (toks[k + 2].kind != LT_NAME || toks[k + 2].text != "_G") continue;
        if (toks[k + 3].kind != LT_OP || toks[k + 3].text != ",") continue;
        if (toks[k + 4].kind == LT_STR) fs.defs.insert(toks[k + 4].text);
    }
}

// ======================== 违规 ========================

struct Violation {
    std::string key;  // 基线条目（B:<file>:<lit> / A:<name> / M:<name>）
    int line;
    std::string detail;
};

bool is_arith_op(const LexTok& t)
{
    return t.kind == LT_OP && t.text.size() == 1
           && (t.text[0] == '+' || t.text[0] == '-' || t.text[0] == '*'
               || t.text[0] == '/' || t.text[0] == '%' || t.text[0] == '^');
}

// 括号包裹的单个字面量：("a") + 1 与 "a" + 1 同错，括号不改变操作数类型。
// 只认"裸括号里恰好一个字符串字面量"；f("a") + 1 里字符串是实参、操作数是
// 调用返回值，那是合法 Lua，不能一起报（dir<0 看左操作数，dir>0 看右）。
const std::string* paren_literal(const std::vector<LexTok>& toks, int i,
                                 int dir)
{
    const int n = (int)toks.size();
    if (i < 0 || i >= n) return nullptr;
    const LexTok& opener = toks[i];
    if (opener.kind != LT_OP || opener.text.size() != 1) return nullptr;

    const char open_ch  = dir < 0 ? ')' : '(';  // 我们正处在的那半个括号
    const char close_ch = dir < 0 ? '(' : ')';  // 要找的配对括号
    if (opener.text[0] != open_ch) return nullptr;

    int depth = 1;
    for (int j = i + dir; j >= 0 && j < n; j += dir) {
        const LexTok& t = toks[j];
        if (t.kind != LT_OP || t.text.size() != 1) continue;
        if (t.text[0] == open_ch) {
            ++depth;
        }
        else if (t.text[0] == close_ch) {
            if (--depth != 0) continue;
            // 配对到了：括号前若是个"值"，说明这是调用/下标而非操作数
            const int before = dir < 0 ? j - 1 : i - 1;
            if (before >= 0) {
                const LexTok& b = toks[before];
                if (b.kind == LT_NAME || b.kind == LT_STR || b.kind == LT_NUM
                    || (b.kind == LT_OP && !b.text.empty()
                        && (b.text[0] == ')' || b.text[0] == ']')))
                    return nullptr;
            }
            const int inner = dir < 0 ? i - j - 1 : j - i - 1;
            if (inner != 1) return nullptr;
            const LexTok& s = toks[dir < 0 ? j + 1 : i + 1];
            return s.kind == LT_STR ? &s.text : nullptr;
        }
    }
    return nullptr;
}

// 字符串字面量参与算术 —— Lua 没有字符串 + 运算符，运行期必抛
// "attempt to add a 'string' with a 'string'"。引擎与数据集均未给字符串挂
// __add 元方法，故字面量参与算术无一例外会炸（实测 15 处全为真故障）。
void detect_string_arith(const std::vector<LexTok>& toks,
                         const std::string& rel_path,
                         std::vector<Violation>& out)
{
    for (size_t k = 0; k < toks.size(); ++k) {
        if (!is_arith_op(toks[k])) continue;
        const std::string* lit = nullptr;
        if (k > 0 && toks[k - 1].kind == LT_STR) {
            lit = &toks[k - 1].text;
        }
        else if (k + 1 < toks.size() && toks[k + 1].kind == LT_STR) {
            lit = &toks[k + 1].text;
        }
        if (!lit && k > 0) lit = paren_literal(toks, (int)k - 1, -1);
        if (!lit && k + 1 < toks.size())
            lit = paren_literal(toks, (int)k + 1, +1);
        if (!lit) continue;

        Violation v;
        v.line   = toks[k].line;
        v.detail = "string literal involved in '" + toks[k].text
                   + "': Lua has no string arithmetic, use '..' to concatenate";
        v.key    = "B:" + rel_path + ":" + key_escape(lit->substr(0, 48));
        out.push_back(v);
    }
}

// ======================== 引擎绑定白名单（读活表） ========================

void collect_table_keys(lua_State* L, int idx, std::set<std::string>& out)
{
    const int t = lua_absindex(L, idx);
    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char* k = lua_tostring(L, -2);
            if (k) out.insert(k);
        }
        lua_pop(L, 1);
    }
}

// _G 的全部键 —— 标准库 + global_funcs + 枚举 + g_Color/START_ROOM/hooks。
// 读活表而非正则扒源码：绑定增减自动跟随，不会漏 lua_setglobal 的间接形态。
void collect_globals(lua_State* L, std::set<std::string>& out)
{
    lua_pushglobaltable(L);
    collect_table_keys(L, -1, out);
    lua_pop(L, 1);
}

// 对象方法 = 注册表里的 "jianghu_object" 元表（__index 指向自身），
// 即 object_methods[] 的全部键 —— self:xxx() 只可能解析到这里。
void collect_object_methods(lua_State* L, std::set<std::string>& out)
{
    lua_getfield(L, LUA_REGISTRYINDEX, "jianghu_object");
    if (lua_istable(L, -1)) collect_table_keys(L, -1, out);
    lua_pop(L, 1);
}

// s:gsub() / t:insert() / f:read() 这类冒号调用走各自标准库元表，
// 与对象方法同为"合法方法名"，一并白名单。
void collect_stdlib_methods(lua_State* L, std::set<std::string>& out)
{
    static const char* kTables[] = {"string", "table",     "io",      "math",
                                    "os",     "coroutine", "package", "debug"};
    for (const char* name : kTables) {
        lua_getglobal(L, name);
        if (lua_istable(L, -1)) collect_table_keys(L, -1, out);
        lua_pop(L, 1);
    }
}

// io 文件句柄的方法（f:seek / f:setvbuf，连同 read/write/close…）挂在 FILE
// 元表上，不在 io 表里——只收 io 表的键会把 f:seek() 误报成未绑定方法。
// 取一个句柄读它的元表即可；NUL 是 Windows 空设备，不落盘。
void collect_file_methods(lua_State* L, std::set<std::string>& out)
{
    lua_getglobal(L, "_check_file_methods");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        lua_pop(L, 1);  // 打不开句柄：按已收集的白名单算，不中断校验
        return;
    }
    if (lua_istable(L, -1)) collect_table_keys(L, -1, out);
    lua_pop(L, 1);
}

// ======================== require 闭包 ========================

// 校验助手（安装到 Lua 全局）：
//  1. _check_require_closure —— 复用引擎 searcher 的 package.path 解析口径，
//     只查找与编译、不执行模块。返回 (true, "") 或 (false, 解析失败原因)。
//  2. _check_file_methods —— 返回 io 文件句柄的元表（见
//  collect_file_methods）。
const char* k_checker_helpers_lua
    = "function _check_require_closure(modname)\n"
      "    local ok, res = pcall(package.searchers[2], modname)\n"
      "    if ok and type(res) == 'function' then return true, '' end\n"
      "    if ok and type(res) == 'string' then return false, res end\n"
      "    return false, tostring(res)\n"
      "end\n"
      "function _check_file_methods()\n"
      "    local ok, f = pcall(io.open, 'NUL', 'r')\n"
      "    if not ok or not f then ok, f = pcall(io.tmpfile) end\n"
      "    if not ok or not f then return nil end\n"
      "    local mt = getmetatable(f)\n"
      "    io.close(f)\n"
      "    if type(mt) ~= 'table' then return nil end\n"
      "    local merged = {}\n"
      "    for k, v in pairs(mt) do merged[k] = v end\n"
      "    local ix = mt.__index\n"
      "    if type(ix) == 'table' and ix ~= mt then\n"
      "        for k, v in pairs(ix) do merged[k] = v end\n"
      "    end\n"
      "    return merged\n"
      "end\n";

// 收集字面量 require 调用的模块名（含 require "mod" 无括号写法）；动态形式
// require(name) / require("a" .. b) 计入 dynamic_count，只记 NOTE 不判失败。
// 注释/字符串内容已被词法器跳过，不会误收集。
void collect_require_calls(const std::vector<LexTok>& toks,
                           std::vector<std::string>& out, int& dynamic_count)
{
    dynamic_count = 0;
    for (size_t k = 0; k < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME || toks[k].text != "require") continue;
        if (k > 0 && toks[k - 1].kind == LT_OP
            && (toks[k - 1].text == "." || toks[k - 1].text == ":"))
            continue;  // t.require(...) 是字段访问，不是 require 调用
        if (k + 1 >= toks.size()) continue;

        const LexTok& arg = toks[k + 1];
        if (arg.kind == LT_STR) {
            out.push_back(arg.text);  // require "mod"：无括号的字面量写法
            continue;
        }
        if (arg.kind != LT_OP || arg.text != "(")
            continue;  // require 只是被当值用
        if (k + 2 >= toks.size()) continue;
        if (toks[k + 2].kind != LT_STR) {
            ++dynamic_count;  // require(name)
            continue;
        }
        // require("a.b") 后面还跟着别的 token → 拼接表达式，模块名非字面量
        if (k + 3 < toks.size()
            && !(toks[k + 3].kind == LT_OP && toks[k + 3].text == ")")) {
            ++dynamic_count;
            continue;
        }
        out.push_back(toks[k + 2].text);
    }
}

// ======================== 每文件结果 ========================

struct FileOutcome {
    bool readable  = true;
    bool syntax_ok = false;
    std::string syntax_err;
    std::string require_err;  // 非空 = 闭包解析失败
    int dynamic_count = 0;
    std::vector<Violation> violations;  // 新增（未被基线豁免）
    int baselined = 0;
};

// 待全量 script_defs 收集完成后消解的候选
struct Cand {
    size_t file_idx;
    int line;
    std::string name;
    bool method;
};

bool violation_less(const Violation& a, const Violation& b)
{
    return a.line < b.line;
}

// 基线：key -> 剩余额度
typedef std::map<std::string, int> Budget;

bool load_baseline(const char* path, Budget& out)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) return false;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        ++out[line];
    }
    fclose(fp);
    return true;
}

}  // namespace

// ======================== 校验入口 ========================

int check_lua_files(lua_State* L, const std::vector<std::string>& files,
                    const CheckLuaOptions* opts)
{
    if (!L) return 1;

    // 自持绑定：没有绑定表就无从判断"这个名字引擎绑没绑"。
    // lua_bind_init 幂等，调用方已初始化也无副作用。
    lua_bind_init(L);

    if (luaL_dostring(L, k_checker_helpers_lua) != LUA_OK) {
        printf("ERROR: cannot install check helpers: %s\n",
               lua_tostring(L, -1));
        lua_pop(L, 1);
        return (int)files.size();
    }

    // 白名单（全部读活表）
    std::set<std::string> g_whitelist;
    std::set<std::string> m_static;  // 对象方法 + 标准库冒号方法
    collect_globals(L, g_whitelist);
    collect_object_methods(L, m_static);
    collect_stdlib_methods(L, m_static);
    collect_file_methods(L, m_static);

    const char* path_prefix = opts ? opts->path_prefix : nullptr;

    std::vector<FileOutcome> outcomes(files.size());
    std::vector<std::string> rel_paths(files.size());
    std::vector<Cand> cands;
    std::set<std::string> all_defs;
    std::set<std::string> all_methods;

    // ---- pass 1：语法 / require / 词法 / 作用域 / 候选 ----
    for (size_t i = 0; i < files.size(); ++i) {
        FileOutcome& o = outcomes[i];
        rel_paths[i]   = make_rel_path(files[i], path_prefix);

        const std::string gbk = load_file_as_gbk(files[i].c_str());
        if (gbk.empty()) {
            o.readable = false;
            continue;
        }

        // 防线 1：语法（编译即校验；通过后丢弃 chunk，不执行）
        if (luaL_loadbuffer(L, gbk.c_str(), gbk.size(), files[i].c_str())
            != LUA_OK) {
            o.syntax_err
                = lua_tostring(L, -1) ? lua_tostring(L, -1) : "syntax error";
            lua_pop(L, 1);
            continue;
        }
        lua_pop(L, 1);
        o.syntax_ok = true;

        std::vector<LexTok> toks;
        lex_lua(gbk, toks);

        std::vector<std::string> modules;
        collect_require_calls(toks, modules, o.dynamic_count);
        for (const std::string& mod : modules) {
            lua_getglobal(L, "_check_require_closure");
            lua_pushlstring(L, mod.c_str(), mod.size());
            if (lua_pcall(L, 1, 2, 0) != LUA_OK) {
                o.require_err = std::string("require check error: ")
                                + lua_tostring(L, -1);
                lua_pop(L, 1);
                break;
            }
            const bool ok = lua_toboolean(L, -2);
            if (!ok) {
                const char* why = lua_tostring(L, -1);
                o.require_err   = "require(\"" + mod
                                  + "\") cannot resolve: " + (why ? why : "");
            }
            lua_pop(L, 2);
            if (!o.require_err.empty()) break;
        }

        FileScan fs;
        analyze_scope(toks, fs);
        all_defs.insert(fs.defs.begin(), fs.defs.end());
        all_methods.insert(fs.methods.begin(), fs.methods.end());

        detect_string_arith(toks, rel_paths[i], o.violations);

        // A1 候选：全局读（先按活 _G 过滤，剩余待全量 script_defs 消解）
        // A2 候选：`x:name(` 冒号调用（先按对象/标准库方法过滤）
        for (size_t k = 0; k < toks.size(); ++k) {
            const LexTok& t = toks[k];
            if (t.kind != LT_NAME) continue;
            const bool has_prev = k > 0;
            const LexTok* prev  = has_prev ? &toks[k - 1] : nullptr;
            const LexTok* next = (k + 1 < toks.size()) ? &toks[k + 1] : nullptr;

            // A2：x:method(
            if (prev && prev->kind == LT_OP && prev->text == ":" && next
                && next->kind == LT_OP && next->text == "(") {
                if (fs.locals.count(t.text)) continue;
                if (m_static.count(t.text)) continue;
                cands.push_back({i, t.line, t.text, true});
                continue;
            }

            if (prev && prev->kind == LT_OP
                && (prev->text == "." || prev->text == ":"))
                continue;
            if (prev && prev->kind == LT_KW
                && (prev->text == "local" || prev->text == "function"
                    || prev->text == "goto"))
                continue;
            if (prev && prev->kind == LT_OP && prev->text == "::") continue;
            if (next && next->kind == LT_OP && next->text == "::")
                continue;  // ::label::
            size_t lhs_end = 0;
            if (assign_lhs(toks, k, lhs_end) > 0)
                continue;  // 赋值目标（含 a, b = ... 多目标）是写入、不是读取
            if (fs.locals.count(t.text)) continue;
            if (g_whitelist.count(t.text)) continue;
            cands.push_back({i, t.line, t.text, false});
        }
    }

    // ---- 定义补充扫描：单文件模式下的跨文件定义 ----
    // 例：city_zeng.lua 用 YOUNG_WOMAN1，它定义在 data/constants.lua；
    // 只扫一个文件时不知道，会误报成"未绑定全局"。
    if (opts) {
        std::set<std::string> scanned(files.begin(), files.end());
        for (const std::string& root : opts->definition_roots) {
            if (root.empty()) continue;
            std::vector<std::string> extra;
            collect_lua_files(root, extra);
            for (size_t i = 0; i < extra.size(); ++i) {
                if (scanned.count(extra[i])) continue;  // 已在待检集里扫过
                scanned.insert(extra[i]);
                const std::string gbk = load_file_as_gbk(extra[i].c_str());
                if (gbk.empty()) continue;
                std::vector<LexTok> toks;
                lex_lua(gbk, toks);
                FileScan fs;
                analyze_scope(toks, fs);
                all_defs.insert(fs.defs.begin(), fs.defs.end());
                all_methods.insert(fs.methods.begin(), fs.methods.end());
            }
        }
    }

    // ---- pass 2：用全量脚本定义消解候选 ----
    for (const Cand& c : cands) {
        if (c.method) {
            if (all_methods.count(c.name)) continue;  // 任一脚本已定义
            Violation v;
            v.line = c.line;
            v.key  = "M:" + c.name;
            v.detail = "method '" + c.name
                       + "' is not bound on the object metatable, not a stdlib "
                         "':'-call method, and not defined by any script";
            outcomes[c.file_idx].violations.push_back(v);
        }
        else {
            if (all_defs.count(c.name)) continue;  // 任一脚本已定义
            Violation v;
            v.line = c.line;
            v.key  = "A:" + c.name;
            v.detail
                = "global '" + c.name
                  + "' is not bound by the engine nor defined by any script";
            outcomes[c.file_idx].violations.push_back(v);
        }
    }

    // ---- 基线 ----
    Budget budget;
    const bool writing_baseline = opts && opts->write_baseline;
    const bool have_baseline = !writing_baseline && opts && opts->baseline_path
                               && load_baseline(opts->baseline_path, budget);

    std::vector<std::string> all_keys;  // 供 --write-baseline 落盘
    for (size_t i = 0; i < files.size(); ++i) {
        FileOutcome& o = outcomes[i];
        std::sort(o.violations.begin(), o.violations.end(), violation_less);
        for (const Violation& v : o.violations) all_keys.push_back(v.key);
        if (!have_baseline) continue;

        std::vector<Violation> fresh;
        for (const Violation& v : o.violations) {
            Budget::iterator it = budget.find(v.key);
            if (it != budget.end() && it->second > 0) {
                --it->second;
                ++o.baselined;
            }
            else {
                fresh.push_back(v);
            }
        }
        o.violations.swap(fresh);
    }

    // ---- 输出 ----
    // 语法/读取失败是基线豁免不掉的硬错误，写基线模式也必须照报：只顾落盘
    // 会把一棵语法写坏的树"洗绿"。require 失败与违规互不遮蔽，一并打印。
    int failed  = 0;
    int success = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        const FileOutcome& o = outcomes[i];
        if (!o.readable) {
            printf("FAIL: %s\n  cannot read file\n", files[i].c_str());
            ++failed;
            continue;
        }
        if (!o.syntax_ok) {
            printf("FAIL: %s\n  %s\n", files[i].c_str(), o.syntax_err.c_str());
            ++failed;
            continue;
        }

        // 写基线时违规交给基线接管，不逐条打印
        const bool show_viol = !writing_baseline && !o.violations.empty();
        if (show_viol || !o.require_err.empty()) {
            printf("FAIL: %s\n", files[i].c_str());
            if (!o.require_err.empty()) {
                printf("  %s\n", o.require_err.c_str());
            }
            if (show_viol) {
                for (const Violation& v : o.violations) {
                    printf("  line %d: %s\n", v.line, v.detail.c_str());
                }
            }
            if (o.dynamic_count > 0) {
                printf(
                    "NOTE: %s: %d dynamic require(...) not statically "
                    "checked\n",
                    files[i].c_str(), o.dynamic_count);
            }
            if (o.baselined > 0) {
                printf("NOTE: %s: %d pre-existing violation(s) baselined\n",
                       files[i].c_str(), o.baselined);
            }
            ++failed;
            continue;
        }
        if (o.dynamic_count > 0) {
            printf("NOTE: %s: %d dynamic require(...) not statically checked\n",
                   files[i].c_str(), o.dynamic_count);
        }
        if (o.baselined > 0) {
            printf("NOTE: %s: %d pre-existing violation(s) baselined\n",
                   files[i].c_str(), o.baselined);
        }
        printf("OK:   %s\n", files[i].c_str());
        ++success;
    }

    // ---- 写基线（存量清理的引导手段；违规本身不判失败）----
    if (writing_baseline) {
        if (!opts->baseline_path) {
            printf("ERROR: write_baseline needs baseline_path\n");
            return (int)files.size();
        }
        std::sort(all_keys.begin(), all_keys.end());
        FILE* fp = fopen(opts->baseline_path, "wb");
        if (!fp) {
            printf("ERROR: cannot write baseline: %s\n", opts->baseline_path);
            return (int)files.size();
        }
        fputs("# --check-lua 基线：存量违规豁免条目（只拦新增）。\n", fp);
        fputs("# 修掉一条就删一条，本文件只减不增。\n", fp);
        for (const std::string& k : all_keys) {
            fputs(k.c_str(), fp);
            fputc('\n', fp);
        }
        fclose(fp);
        printf("BASELINE: wrote %d entr%s to %s\n", (int)all_keys.size(),
               all_keys.size() == 1 ? "y" : "ies", opts->baseline_path);
    }

    // 基线残留 = 已修好，提示清理（不判失败）
    if (have_baseline) {
        int stale = 0;
        for (Budget::iterator it = budget.begin(); it != budget.end(); ++it)
            if (it->second > 0) stale += it->second;
        if (stale > 0) {
            printf("NOTE: baseline: %d entr%s already fixed - remove from %s\n",
                   stale, stale == 1 ? "y" : "ies", opts->baseline_path);
        }
    }

    printf("\n%d file(s) OK, %d file(s) FAILED\n", success, failed);
    return failed;
}

// ======================== 命令行入口 ========================

int run_lua_checker(const char* cmdLine)
{
    const char* p = strstr(cmdLine, "--check-lua");
    if (!p) {
        printf("ERROR: --check-lua flag found but cannot parse\n");
        return 1;
    }
    p = skip_arg(p + strlen("--check-lua"));

    std::string configPath = "data/config.ini";
    std::string dirPath;
    std::string baselinePath;
    bool writeBaseline = false;
    std::vector<std::string> files;

    // 旗标顺序无关；其余参数视为待检文件
    while (*p) {
        p = skip_arg(p);
        if (!*p) break;
        if (strncmp(p, "--config", 8) == 0) {
            configPath = extract_arg(p = skip_arg(p + 8));
        }
        else if (strncmp(p, "--dir", 5) == 0) {
            dirPath = extract_arg(p = skip_arg(p + 5));
        }
        else if (strncmp(p, "--baseline", 10) == 0) {
            baselinePath = extract_arg(p = skip_arg(p + 10));
        }
        else if (strncmp(p, "--write-baseline", 16) == 0) {
            p += 16;
            writeBaseline = true;
        }
        else {
            files.push_back(extract_arg(p));
        }
    }

    if (dirPath.empty() && files.empty()) {
        printf(
            "usage: jianghu.exe --check-lua [--config <ini>] [--dir <path>]\n"
            "                    [--baseline <file>] [--write-baseline]\n"
            "                    <file.lua> ...\n");
        return 1;
    }
    // 写基线必须知道往哪写：既无 --dir（提供缺省位置）也无 --baseline
    if (writeBaseline && dirPath.empty()) {
        printf(
            "ERROR: --write-baseline needs --dir <path> or --baseline "
            "<file>\n");
        return 1;
    }

    // --dir 打错 / 目录里一个 .lua 都没有：不能当"全部通过"，门禁会假绿。
    // 这里先收集再校验，避免走到最后打出 "0 file(s) OK, 0 file(s) FAILED"
    // 就退出。收集不依赖配置与引擎，放在 ini_load 之前。
    if (!dirPath.empty()) collect_lua_files(dirPath, files);
    if (files.empty()) {
        const DWORD attr = GetFileAttributesA(dirPath.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES
            && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            printf("ERROR: no .lua file under --dir %s\n", dirPath.c_str());
        }
        else {
            printf("ERROR: --dir does not exist: %s\n", dirPath.c_str());
        }
        return 1;
    }

    ini_load(configPath.c_str());

    CLuaEngine* engine      = CLuaEngine::instance();
    const char* luaRoot     = ini_get("lua", "script_root", "lua");
    const char* defaultRoot = ini_get("lua", "default_script_root", "");
    if (!engine->init(luaRoot, defaultRoot)) {
        printf("ERROR: Lua engine init failed\n");
        return 1;
    }
    lua_State* L = engine->get_state();

    // 未显式指定时，基线随 --dir 落在被检根目录下。
    // 写模式同样需要这个位置——正是它决定往哪写。
    if (baselinePath.empty() && !dirPath.empty()) {
        baselinePath = dirPath + "\\check_lua_baseline.txt";
    }

    CheckLuaOptions opts;
    opts.path_prefix    = dirPath.empty() ? nullptr : dirPath.c_str();
    opts.baseline_path  = baselinePath.empty() ? nullptr : baselinePath.c_str();
    opts.write_baseline = writeBaseline;
    // 跨文件定义的来源：两个脚本根都扫，单文件模式才不会把脚本自定义全局
    // （constants.lua 的图标常量、damage_msg.lua 的全局函数）误报成未绑定
    {
        const char* roots[2] = {ini_get("lua", "script_root", ""),
                                ini_get("lua", "default_script_root", "")};
        for (const char* r : roots) {
            if (r && *r) opts.definition_roots.push_back(r);
        }
    }

    const int failed = check_lua_files(L, files, &opts);

    engine->shutdown();
    return failed ? 1 : 0;
}

/** @file LuaSyntaxChecker.cpp
 *  @brief --check-lua 静态校验实现。
 *
 *  各道防线与静态违规检测，各自对应一次真实线上故障（都是先炸后发现 → 逐道
 *  前移到启动前）：
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
 *                   接收者为 self 时口径更严：self 恒为引擎压入的对象
 *                   userdata，能取到的只有 jianghu_object 元表的键，脚本表
 *                   字段（function npc.foo）经 self:foo() 不可达——「某个
 *                   脚本里定义过」放行只会放走必炸的调用。
 *  5. call_out 目标 数据集 xy_tielao 系把延迟函数写成顶层裸 function（全局）、
 *                   hangzhou_lieyandao 的 do_time 同——名字不在发起脚本的返回
 *                   表里，注册期按名两处都查不到即 LOG_ERROR 拒绝，回调根本
 *                   挂不上。规则见 docs/guide/call_out.md（规则1：写成
 *                   模块名.名字）。
 *  6. 钩子冒烟      （默认开）静态只能证明"名字存在"，这道把引擎本来就会
 *                   做的调用真跑一遍：item 的 create / do_use。钩子抛出的
 *                   错误（引擎侧 pcall 吞掉只写日志）、被脚本自己的 pcall
 *                   吞掉的错误、模块体执行期错误，一律记为违规。实现见
 *                   LuaSmokeCheck.cpp。
 *
 *  7. random 语义    全局 random()（已废弃，0 基 [0, n-1]，见 Random.cpp）与
 *                   math.random（1 基 [1, n]）混用：t[random(i)] 取到 nil、
 *                   random(mp / 2) 抛 "number has no integer representation"、
 *                   math.random(0) 回绕成全量域大整数。教材根（lua-default /
 *                   lua-sample）另禁用裸 random()，不留已废弃 API 的示范。
 *  8. 整型实参      绑定形参走 luaL_checkinteger 时，实参必须有整数表示：
 *                   start_busy(skill / 50 + 2)、add("mp", -mp / 2) 这类 '/'
 *                   产出 float，不被整除即抛 "number has no integer
 *                   representation"（数据集 lingboweibu 的线上故障）。直接
 *                   传入与先存局部再传入两条路径都判，string.format 的 %d
 *                   转换符同判；绑定清单与整型位置见 kIntegerArgSpecs。
 *  9. 模块表字段    字面量 require 绑定的局部调用模块没导出的字段，运行期抛
 *                   "attempt to call a nil value (field 'x')"（数据集
 *                   mingjiao_db 把 on_mj_special 写成全局函数，35 处
 *                   mj_db.on_mj_special(...) 全取到 nil）。与模块自己的导出
 *                   面对账，不看"语料里有没有人定义过这个名字"。
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
 *      例外已补：`local X = { ... }` 构造器区间单独识别——X 当时还没有作用域，
 *      区间内的引用必然是全局读，无条件判违规。
 *   3. 方法消解只看"全语料有没有人定义过这个名字"，不校验目标对象上真的
 *      挂得到，跨对象同名会被放行。
 *   4. 防线5 只检 call_out/call_out_combat 的**字面量**注册名（非字面量实参
 *      静态不可判，跳过）；remove_call_out 的配对与死移除不在本道。
 *   5. 防线7/8 的实参只判 float 除法 '/'：字面量浮点（random(2.5)）、'^'、由
 *      函数返回的 float 不判；'//' 与 math.floor(...)/math.modf(...) 内的 '/'
 *      视为整型。防线8 的局部变量追踪是文件平面的：同名局部、跨函数同名按
 *      宁可漏的口径跳过（与盲区2 同源的取舍）。
 *   6. 防线9 只跟踪字面量 require 直接绑定的局部：别名转发（local a = m）、
 *      经字段保存的 require、m.sub.fn() 的 fn、require "lit".field 无括号
 *      直链都追不到接收者，不判；模块导出面静态不可判（动态键 / 元表 /
 *      返回值不是表 / 分支返回）时整模块不报。
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "LuaSyntaxChecker.h"
#include "LuaSmokeCheck.h"
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
    TokKind kind = LT_NAME;
    std::string text;
    int line = 0;
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
    // `local X = { ... }` 的构造器区间（[local 词元, 配对 } 词元)）。
    // 局部变量的作用域从声明语句**之后**才开始，区间内的 X 解析成全局
    // （运行期为 nil）——候选收集据此不放行。
    std::map<std::string, std::vector<std::pair<size_t, size_t>>> init_scope;
};

// 词元 k 是否落在 `local name = { ... }` 自己的构造器里
bool inside_own_init(const FileScan& fs, const std::string& name, size_t k)
{
    auto it = fs.init_scope.find(name);
    if (it == fs.init_scope.end()) return false;
    for (const auto& range : it->second)
        if (range.first < k && k < range.second) return true;
    return false;
}

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
            // local X = { ... }：登记构造器区间。区间内引用 X 拿不到局部
            // 变量（作用域从本语句之后才开始），读到的是全局。
            if (m + 1 < n && toks[m].kind == LT_OP && toks[m].text == "="
                && toks[m + 1].kind == LT_OP && toks[m + 1].text == "{") {
                size_t depth = 0;
                size_t e     = m + 1;
                for (; e < n; ++e) {
                    if (toks[e].kind != LT_OP) continue;
                    if (toks[e].text == "{")
                        ++depth;
                    else if (toks[e].text == "}" && --depth == 0)
                        break;
                }
                if (e < n) {
                    // 区间从 `=` 起算：声明语句里的名字 token 本身不算引用，
                    // 否则 `local X = {` 这行会把 X 自己报出来。
                    for (size_t d = k + 1; d <= m && d < n; ++d)
                        if (toks[d].kind == LT_NAME)
                            fs.init_scope[toks[d].text].push_back({m, e});
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
        // 别名导出：X.f = g 或构造器内 { f = g } —— 字段真的在表里，
        // 按名 call_out 就查得到（docs/guide/call_out.md 规则1 的合法形态）
        const bool rhs_name = eq + 1 < n && toks[eq + 1].kind == LT_NAME;

        if (member_write) {
            // _G.name = ... 与 name = ... 等价：漏记定义会让别处的裸读被
            // 误报成未绑定全局（_G.x = 1 不是表字段、也不是本地名）
            const bool via_g = k > 1 && toks[k - 2].kind == LT_NAME
                               && toks[k - 2].text == "_G";
            if (via_g) {
                fs.defs.insert(t.text);
            }
            else if (rhs_fn || rhs_name) {
                // T.name = function(...) —— 挂在别的表上，属于脚本方法；
                // T.name = g 是别名导出，字段同样在表里。其余写入与方法无关
                fs.methods.insert(t.text);
            }
            continue;
        }

        if (is_field[k]) {
            // 表构造器内的 { name = function(...) } —— 脚本表方法；
            // { name = g } 别名导出同理。先于 locals 判定：与局部同名不改变
            // 「字段在表里」这一事实（chunjie_yh 的 local function + 表内导出
            // 正是这个形态）
            if (rhs_fn || rhs_name) fs.methods.insert(t.text);
            continue;
        }

        if (fs.locals.count(t.text)) continue;

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
    std::string key;  // 基线条目（B:<file>:<lit> / A:<name> / M:<name> /
                      // C:<file>:<name>）
    int line = 0;
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

// ======================== 防线 7：random 语义 ========================
//
// 引擎全局 random() 走 l_random（src/util/Random.cpp，已废弃）：返回 [0, n-1]、
// 实参经 luaL_checkinteger 拒收非整型 float；Lua 标准库 math.random 返回
// [1, n]。两套语义在语料里并存，混用即线上故障——actions[random(i)] 取到
// t[0] = nil；random(mp / 2) 在 mp 为奇数时抛 "number has no integer
// representation"；math.random(0) 把空区间回绕成全量域大整数。本道只判写法与
// 所选语义是否自洽，不做取值分析：
//   R1 0 基下标     t[random(...)]，或裸 random() 存进局部后 t[name]
//   R2 实参含 '/'   '/' 结果是 float，多半无整数表示（'//' 与 math.floor 的
//                   实参除外）
//   R3 math.random  与字面量 0 比较恒真/恒假；单参 0/负数
//   R4 教材根禁用   lua-default / lua-sample 是新脚本的范本，不留已废弃 API
//
enum RandKind { RAND_NONE, RAND_ENGINE, RAND_MATH };

// 裸 random() 的赋值点：line 供报错引用，tok 供作用域判定（跨函数同名的局部
// 是两个不同的变量，不能当成同一条数据流）
struct BareRandomLocal {
    int line   = 0;
    size_t tok = 0;
};

// k 处是被调用的名字：RAND_ENGINE = 裸 random(...)，RAND_MATH =
// math.random(...)
RandKind rand_call_kind(const std::vector<LexTok>& toks, size_t k)
{
    if (toks[k].kind != LT_NAME || toks[k].text != "random") return RAND_NONE;
    if (k + 1 >= toks.size() || toks[k + 1].kind != LT_OP
        || toks[k + 1].text != "(")
        return RAND_NONE;
    if (k > 0 && toks[k - 1].kind == LT_OP
        && (toks[k - 1].text == "." || toks[k - 1].text == ":")) {
        return (k >= 2 && toks[k - 2].kind == LT_NAME
                && toks[k - 2].text == "math")
                   ? RAND_MATH
                   : RAND_NONE;  // t.random(...) / obj:random(...)
                                 // 不是我们要判的
    }
    return RAND_ENGINE;
}

// open 指向 '('，返回配对 ')' 的下标；无配对返回 toks.size()
size_t match_paren(const std::vector<LexTok>& toks, size_t open)
{
    int depth = 0;
    for (size_t j = open; j < toks.size(); ++j) {
        if (toks[j].kind != LT_OP) continue;
        if (toks[j].text == "(") {
            ++depth;
        }
        else if (toks[j].text == ")") {
            if (--depth == 0) return j;
        }
    }
    return toks.size();
}

// open 指向 '['，返回配对 ']' 的下标；无配对返回 toks.size()
size_t match_bracket(const std::vector<LexTok>& toks, size_t open)
{
    int depth = 0;
    for (size_t j = open; j < toks.size(); ++j) {
        if (toks[j].kind != LT_OP) continue;
        if (toks[j].text == "[") {
            ++depth;
        }
        else if (toks[j].text == "]") {
            if (--depth == 0) return j;
        }
    }
    return toks.size();
}

// open 指向 '{'，返回配对 '}' 的下标；无配对返回 toks.size()
size_t match_brace(const std::vector<LexTok>& toks, size_t open)
{
    int depth = 0;
    for (size_t j = open; j < toks.size(); ++j) {
        if (toks[j].kind != LT_OP) continue;
        if (toks[j].text == "{") {
            ++depth;
        }
        else if (toks[j].text == "}") {
            if (--depth == 0) return j;
        }
    }
    return toks.size();
}

// 实参区间 (open, close) 内是否出现 float 除法 '/'。math.floor(...) 与
// math.modf(...) 的实参除外：floor 取整、modf 取整数部分，结果都恒有整数
// 表示；'//' 是另一个 token，本函数不认它。
bool has_float_division(const std::vector<LexTok>& toks, size_t open,
                        size_t close)
{
    size_t floor_close = 0;
    for (size_t j = open + 1; j < close; ++j) {
        if (j < floor_close) continue;
        const LexTok& t = toks[j];
        if (t.kind == LT_NAME && (t.text == "floor" || t.text == "modf")
            && j >= 2 && j + 1 < close && toks[j - 1].kind == LT_OP
            && toks[j - 1].text == "." && toks[j - 2].kind == LT_NAME
            && toks[j - 2].text == "math" && toks[j + 1].kind == LT_OP
            && toks[j + 1].text == "(") {
            floor_close = match_paren(toks, j + 1);
            continue;
        }
        if (t.kind == LT_OP && t.text == "/") return true;
    }
    return false;
}

// 教材根：新脚本的范本，按路径判定。先补一个前导 '\'：--dir 既可以写绝对路径
// 也可以写相对路径（`--dir lua-default` 收集到的就是 `lua-default\x.lua`），
// 两种写法都要落在同一个判定上。
bool under_textbook_root(const std::string& path)
{
    std::string s = "\\" + path;
    std::replace(s.begin(), s.end(), '/', '\\');
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return s.find("\\lua-default\\") != std::string::npos
           || s.find("\\lua-sample\\") != std::string::npos;
}

// 裸 random() 的结果直接落进变量（R1 的数据流起点）：`local NAME = random(...)`
// 与 `NAME = random(...)`。赋值后还接运算符/字面量的不是裸值（`random(i) + 1`
// 早已是 1 基），跳过。NAME 是表字段还是语句赋值静态难分，只认本文件声明过的
// 局部（宁可漏、不可误报）。
void collect_bare_random_locals(const std::vector<LexTok>& toks,
                                const FileScan& fs,
                                std::map<std::string, BareRandomLocal>& out)
{
    for (size_t k = 0; k + 3 < toks.size(); ++k) {
        size_t name = 0;
        size_t eq   = 0;
        if (toks[k].kind == LT_KW && toks[k].text == "local") {
            if (toks[k + 1].kind != LT_NAME || toks[k + 2].kind != LT_OP
                || toks[k + 2].text != "=")
                continue;
            name = k + 1;
            eq   = k + 2;
        }
        else if (toks[k].kind == LT_NAME && toks[k + 1].kind == LT_OP
                 && toks[k + 1].text == "=" && fs.locals.count(toks[k].text)
                 && (k == 0 || toks[k - 1].kind != LT_OP
                     || (toks[k - 1].text != "." && toks[k - 1].text != "]"))) {
            name = k;
            eq   = k + 1;
        }
        else {
            continue;
        }
        const size_t call = eq + 1;
        if (call + 1 >= toks.size()
            || rand_call_kind(toks, call) != RAND_ENGINE)
            continue;
        const size_t close = match_paren(toks, call + 1);
        if (close == toks.size()) continue;
        if (close + 1 < toks.size()) {
            const LexTok& next = toks[close + 1];
            if (next.kind == LT_OP || next.kind == LT_NUM
                || next.kind == LT_STR)
                continue;
            if (next.text == "(" || next.text == "{" || next.text == "[")
                continue;
        }
        out.emplace(toks[name].text, BareRandomLocal{toks[name].line, name});
    }
}

// 0 基表的构造证据（有证据才豁免，其余照报）：
//   1. `for NAME = 0, ...` 的循环变量被当下标（dice 的 counts、mpms 的 noacc）
//   2. 字面量 0 下标 `T[0]`
//   3. 显式 0 基键 `T = { [0] = ..., [1] = ... }`（wanjie / zaixiang
//   的通缉怪表，
//      表头注释即写明"0 基索引，对应 C++ monster[N]"）
// 对这种表用 0 基 random 下标是正确写法，不判。
void collect_zero_based_tables(const std::vector<LexTok>& toks,
                               std::set<std::string>& out)
{
    std::set<std::string> zero_vars;
    for (size_t k = 0; k + 3 < toks.size(); ++k) {
        if (toks[k].kind != LT_KW || toks[k].text != "for") continue;
        if (toks[k + 1].kind != LT_NAME) continue;
        if (toks[k + 2].kind != LT_OP || toks[k + 2].text != "=") continue;
        if (toks[k + 3].kind != LT_NUM || toks[k + 3].text != "0") continue;
        zero_vars.insert(toks[k + 1].text);
    }
    for (size_t k = 1; k + 1 < toks.size(); ++k) {
        if (toks[k].kind != LT_OP || toks[k].text != "[") continue;
        if (toks[k - 1].kind != LT_NAME) continue;
        const bool zero_literal
            = toks[k + 1].kind == LT_NUM && toks[k + 1].text == "0";
        if (!zero_literal && !zero_vars.count(toks[k + 1].text)) continue;
        out.insert(toks[k - 1].text);
    }
    for (size_t k = 0; k + 2 < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME) continue;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "=") continue;
        if (toks[k + 2].kind != LT_OP || toks[k + 2].text != "{") continue;
        const size_t close = match_brace(toks, k + 2);
        if (close == toks.size()) continue;
        for (size_t j = k + 3; j + 2 < close; ++j) {
            if (toks[j].kind == LT_OP && toks[j].text == "["
                && toks[j + 1].kind == LT_NUM && toks[j + 1].text == "0"
                && toks[j + 2].kind == LT_OP && toks[j + 2].text == "]") {
                out.insert(toks[k].text);
                break;
            }
        }
    }
}

// R1：0 基下标 —— 直接 t[random(...)]，或 t[name] 且 name 是裸 random() 的结果
void detect_random_index(
    const std::vector<LexTok>& toks, const std::string& rel, const FileScan& fs,
    const std::map<std::string, BareRandomLocal>& bare_locals,
    std::vector<Violation>& out)
{
    // 脚本自己定义/遮蔽了 random（如 local random = math.random），0 基判定
    // 无从谈起，跳过本条（R2 与两种 API 都要求整型实参，不受影响）
    if (fs.locals.count("random") || fs.defs.count("random")) return;
    std::set<std::string> zero_tables;
    collect_zero_based_tables(toks, zero_tables);
    for (size_t k = 0; k + 1 < toks.size(); ++k) {
        if (toks[k].kind != LT_OP || toks[k].text != "[") continue;
        const size_t close = match_bracket(toks, k);
        if (close == toks.size() || close < k + 2) continue;
        // 目标表有 0 基构造证据 → 0 基下标正是它要的写法
        if (k > 0 && toks[k - 1].kind == LT_NAME
            && zero_tables.count(toks[k - 1].text))
            continue;

        if (rand_call_kind(toks, k + 1) == RAND_ENGINE
            && match_paren(toks, k + 2) == close - 1) {
            Violation v;
            v.line = toks[k + 1].line;
            v.key  = "R:" + rel + ":idx";
            v.detail
                = "engine random() is 0-based ([0, n-1]); using it as a"
                  " table index reads t[0] = nil. Write random(n) + 1, or"
                  " use math.random(n) which is 1-based";
            out.push_back(v);
            continue;
        }
        if (close != k + 2 || toks[k + 1].kind != LT_NAME) continue;
        const auto it = bare_locals.find(toks[k + 1].text);
        if (it == bare_locals.end()) continue;
        // 跨函数同名：赋值行在使用行之后，或两处之间隔着 function 关键词，说明
        // 这里读的是另一个局部（数据集 mfnpc_1 就是 for 循环变量 i 撞了同名
        // 局部）—— 不判，宁可漏、不可误报。
        if (k < it->second.tok) continue;
        bool cross_function = false;
        for (size_t j = it->second.tok + 1; j < k; ++j) {
            if (toks[j].kind == LT_KW && toks[j].text == "function") {
                cross_function = true;
                break;
            }
        }
        if (cross_function) continue;
        Violation v;
        v.line = toks[k + 1].line;
        v.key  = "R:" + rel + ":idxvar:" + toks[k + 1].text;
        v.detail = "'" + toks[k + 1].text
                   + "' is a raw random() result (0-based, assigned on line "
                   + std::to_string(it->second.line) + "); indexing with it reads"
                     " t[0] = nil. Add +1 at the assignment";
        out.push_back(v);
    }
}

// R2：实参含 '/' → float，多半没有整数表示（luaL_checkinteger 拒收）
void detect_random_float_arg(const std::vector<LexTok>& toks,
                             const std::string& rel,
                             std::vector<Violation>& out)
{
    for (size_t k = 0; k < toks.size(); ++k) {
        const RandKind kind = rand_call_kind(toks, k);
        if (kind == RAND_NONE) continue;
        const size_t open  = k + 1;
        const size_t close = match_paren(toks, open);
        if (close == toks.size() || !has_float_division(toks, open, close))
            continue;
        Violation v;
        v.line = toks[k].line;
        v.key  = "R:" + rel + ":intarg";
        v.detail = std::string(kind == RAND_MATH ? "math.random" : "random")
                   + " takes an integer argument, but '/' in the argument"
                     " yields a float that may have no integer representation"
                     " (runtime: number has no integer representation). Use"
                     " '//' or wrap it in math.floor(...)";
        out.push_back(v);
    }
}

// R3a：单参且为字面量（可带一元负号）的 math.random —— 0 不是"空一点的区间"
void detect_math_random_bound(const std::vector<LexTok>& toks,
                              const std::string& rel,
                              std::vector<Violation>& out)
{
    for (size_t k = 0; k < toks.size(); ++k) {
        if (rand_call_kind(toks, k) != RAND_MATH) continue;
        const size_t open  = k + 1;
        const size_t close = match_paren(toks, open);
        if (close == toks.size()) continue;
        const size_t argc = close - open - 1;
        bool neg          = false;
        size_t lit        = 0;
        if (argc == 1 && toks[open + 1].kind == LT_NUM) {
            lit = open + 1;
        }
        else if (argc == 2 && toks[open + 1].kind == LT_OP
                 && toks[open + 1].text == "-"
                 && toks[open + 2].kind == LT_NUM) {
            neg = true;
            lit = open + 2;
        }
        if (lit == 0) continue;
        char* end         = nullptr;
        const long long n = strtoll(toks[lit].text.c_str(), &end, 0);
        if (!end || *end != '\0') continue;
        const long long bound = neg ? -n : n;
        if (bound > 0) continue;
        Violation v;
        v.line = toks[k].line;
        v.key  = "R:" + rel + ":mrand0";
        v.detail = bound == 0
                       ? std::string("math.random(0) does not return 0: Lua 5.5")
                             + " wraps the empty interval into a full-range"
                             + " integer. Use a literal 0, or math.random(0, n)"
                                 " for a 0-based range"
                       : std::string("math.random with a negative bound raises")
                             + " 'interval is empty' at runtime";
        out.push_back(v);
    }
}

// R3b：单参 math.random 落在 [1, n]，与字面量 0 比较的结果是常量
void detect_math_random_compare(const std::vector<LexTok>& toks,
                                const std::string& rel,
                                std::vector<Violation>& out)
{
    for (size_t k = 0; k < toks.size(); ++k) {
        if (rand_call_kind(toks, k) != RAND_MATH) continue;
        const size_t open  = k + 1;
        const size_t close = match_paren(toks, open);
        if (close == toks.size()) continue;
        if (close - open - 1 != 1)
            continue;  // 两参形式（math.random(0,1)）可取 0
        const bool right = close + 2 < toks.size()
                           && toks[close + 2].kind == LT_NUM
                           && toks[close + 2].text == "0";
        const bool left
            = k >= 4 && toks[k - 4].kind == LT_NUM && toks[k - 4].text == "0";
        if (!right && !left) continue;
        const LexTok& op             = right ? toks[close + 1] : toks[k - 3];
        static const char* kCmpOps[] = {"==", "~=", "<", "<=", ">", ">="};
        if (op.kind != LT_OP
            || std::find(std::begin(kCmpOps), std::end(kCmpOps), op.text)
                   == std::end(kCmpOps))
            continue;
        const bool always
            = op.text == "~=" || op.text == ">" || op.text == ">=";
        Violation v;
        v.line   = toks[k].line;
        v.key    = "R:" + rel + ":mrandcmp";
        v.detail = "math.random with one argument returns at least 1, so '"
                   + op.text + " 0' is "
                   + (always ? "always true" : "never true")
                   + " (0-based habit applied to a 1-based function)";
        out.push_back(v);
    }
}

// R4：教材根禁用已废弃的裸 random()
void detect_textbook_random(const std::vector<LexTok>& toks,
                            const std::string& abs_path, const std::string& rel,
                            std::vector<Violation>& out)
{
    if (!under_textbook_root(abs_path)) return;
    for (size_t k = 0; k < toks.size(); ++k) {
        if (rand_call_kind(toks, k) != RAND_ENGINE) continue;
        Violation v;
        v.line = toks[k].line;
        v.key  = "R:" + rel + ":textbook";
        v.detail
            = "the deprecated global random() must not appear in textbook"
              " scripts (lua-default / lua-sample); use math.random, whose"
              " [1, n] range is the Lua standard";
        out.push_back(v);
    }
}

void detect_random_misuse(const std::vector<LexTok>& toks,
                          const std::string& abs_path, const std::string& rel,
                          const FileScan& fs, std::vector<Violation>& out)
{
    std::map<std::string, BareRandomLocal> bare_locals;
    collect_bare_random_locals(toks, fs, bare_locals);
    detect_random_index(toks, rel, fs, bare_locals, out);
    detect_random_float_arg(toks, rel, out);
    detect_math_random_bound(toks, rel, out);
    detect_math_random_compare(toks, rel, out);
    detect_textbook_random(toks, abs_path, rel, out);
}

// ======================== 防线 8：整型实参 ========================
//
// 绑定形参走 luaL_checkinteger / luaL_optinteger 时，实参必须有整数表示：
// '/' 产出 float，x / 3 在 x 不被整除时直接抛 "number has no integer
// representation"（数据集 lingboweibu 的 start_busy 线上故障，报错形如
// cast: Lua skill cast chan failed ... bad argument #1 to 'start_busy'）。
// 防线7 只判 random / math.random 的实参，其余整型绑定归本道；string.format
// 的 %d/%i/%o/%u/%x 转换符按整型实参同判。
//
// 只判实参里的 '/'：'//' 是另一个 token；math.floor(...) 与 math.modf(...)
// 的结果恒有整数表示，其内部的 '/' 由 has_float_division 一并放行。字面量
// 浮点、'^'、函数返回的 float 不判。
//
// 间接形态（先存局部、再传给绑定）按 token 顺序追踪：赋值语句的右侧扫完才
// 更新状态，语句右侧的读取看到的是赋值前的值；后续非 float 赋值清除标记；
// 赋值与使用之间隔着 function 的同名局部视为两个变量（与 R1 同口径）——
// 宁可漏、不可误报。

// 整型实参绑定表：绑定名 → 整型的 L 位置（1 起，方法调用含 self）。
// random 归防线7（R2），不在此表；string.format 的位置随格式串变化，单独处理。
// 每行 {"name", "位置,位置"} 的写法是与源码对账的契约：
// tests/unit/test_lua_checker.cpp::IntegerArgBindingsReconcileWithSource 解析
// 本表、逐条比对 src/ 里的 luaL_checkinteger 位置，新增整型绑定而未登记即失败。
struct IntegerArgSpec {
    const char* name;
    const char* l_positions;  // 逗号分隔的 L 位置
};

static const IntegerArgSpec kIntegerArgSpecs[] = {
    {"add", "3"},
    {"add_balance", "2"},
    {"add_magic", "4,5,6,7,9"},
    {"add_money", "2"},
    {"add_object", "4"},
    {"apply_condition", "3,4"},
    {"apprentice", "4"},
    {"base_query", "2"},
    {"begin_fb", "3,4"},
    {"begin_fight", "2,3"},
    {"call_out", "3,4,5"},
    {"call_out_combat", "3,4,5"},
    {"can_join", "2"},
    {"carry_good", "3,4"},
    {"carry_object", "3"},
    {"combat_attack", "4,5"},
    {"create", "2"},
    {"create_family", "3"},
    {"desc", "3"},
    {"do_action_base", "2"},
    {"do_bishi", "4"},
    {"do_emote", "4"},
    {"find", "2,3"},
    {"get_fight_team_char", "2"},
    {"get_team_list", "2,4"},
    {"global_room_name", "1"},
    {"id", "2"},
    {"improve_skill", "3,4"},
    {"inc_skill", "3"},
    {"join_team", "3"},
    {"load", "3"},
    {"lock_data", "2"},
    {"locked", "3"},
    {"move_in", "3"},
    {"name", "2"},
    {"pay_money", "3"},
    {"perform_action", "3"},
    {"present", "3,4,5"},
    {"query_skill", "3"},
    {"receive_curing", "3"},
    {"receive_damage", "3"},
    {"receive_healing", "3"},
    {"receive_poison", "3"},
    {"receive_wound", "3"},
    {"register_char", "5,6,7,8"},
    {"remove_powerup", "2,3"},
    {"revive", "2"},
    {"script_exists", "2"},
    {"select_member", "2"},
    {"set_amount", "2"},
    {"set_heart_beat", "2"},
    {"set_skill", "3"},
    {"set_weight", "2"},
    {"split", "2"},
    {"start_busy", "2"},
    {"start_powerup", "3,4,5"},
    {"throw_weapon", "4"},
    {"unlocked", "2"},
    {"update_recruit", "4,5"},
    {"xrandom", "1"},
};

const std::map<std::string, std::set<int>>& integer_arg_bindings()
{
    static const std::map<std::string, std::set<int>> table = [] {
        std::map<std::string, std::set<int>> t;
        for (const IntegerArgSpec& s : kIntegerArgSpecs) {
            std::set<int> positions;
            const char* p = s.l_positions;
            while (*p) {
                positions.insert(atoi(p));
                while (*p && *p != ',') ++p;
                if (*p == ',') ++p;
            }
            t[s.name] = std::move(positions);
        }
        return t;
    }();
    return table;
}

// 实参按顶层逗号切分：每个实参给出 [begin, end) 的 token 下标。
void split_call_args(const std::vector<LexTok>& toks, size_t open, size_t close,
                     std::vector<std::pair<size_t, size_t>>& out)
{
    size_t depth = 0;
    size_t begin = open + 1;
    for (size_t j = open + 1; j < close; ++j) {
        const LexTok& t = toks[j];
        if (t.kind != LT_OP) continue;
        if (t.text == "(" || t.text == "[" || t.text == "{") {
            ++depth;
        }
        else if (t.text == ")" || t.text == "]" || t.text == "}") {
            if (depth > 0) --depth;
        }
        else if (t.text == "," && depth == 0) {
            out.push_back({begin, j});
            begin = j + 1;
        }
    }
    if (begin < close) out.push_back({begin, close});
}

// 表达式接续 token：换行处出现它，说明表达式还没写完（Lua 没有自动分号，
// `x\n - 2` 就是 x - 2），多行 RHS 不在此断句。
bool is_expr_continuation(const LexTok& t)
{
    if (t.kind == LT_KW) return t.text == "and" || t.text == "or";
    if (t.kind != LT_OP) return false;
    static const char* kOps[] = {
        "+", "-", "*", "/", "%", "^", "==", "~=", "<", ">", "<=", ">=", ".."};
    return std::find(std::begin(kOps), std::end(kOps), t.text)
           != std::end(kOps);
}

// 赋值语句右侧的结束位置（最后一个 token 的下标 + 1）：深度 0 处的 ';'、
// 或换行后既不以运算符接续也不以运算符开头，即语句到此为止。
size_t assign_rhs_end(const std::vector<LexTok>& toks, size_t eq)
{
    int depth  = 0;
    int line   = toks[eq].line;
    size_t end = eq + 1;
    for (size_t j = eq + 1; j < toks.size(); ++j) {
        const LexTok& t = toks[j];
        if (t.kind == LT_OP) {
            if (t.text == "(" || t.text == "[" || t.text == "{") {
                ++depth;
            }
            else if (t.text == ")" || t.text == "]" || t.text == "}") {
                if (depth > 0) --depth;
            }
            else if (t.text == ";" && depth == 0) {
                return j;
            }
        }
        if (depth == 0 && t.line > line) {
            const LexTok& prev = toks[j - 1];
            if (!is_expr_continuation(prev) && !is_expr_continuation(t)
                && t.text != "(" && t.text != "[" && t.text != "{") {
                return j;
            }
        }
        end  = j + 1;
        line = t.line;
    }
    return end;
}

struct IntegerArgEvent {
    size_t pos = 0;    // 排序位：赋值取 RHS 末尾，实参取调用名
    int kind   = 0;    // 0 = 赋值, 1 = 实参读取
    std::string name;  // 赋值：变量名；读取：绑定名
    bool float_rhs   = false;
    size_t decl_tok  = 0;  // 赋值：变量名所在 token（跨函数判定用）
    int decl_line    = 0;
    size_t arg_begin = 0;
    size_t arg_end   = 0;
    int line         = 0;
    int lpos         = 0;
};

void detect_integer_arg_float(const std::vector<LexTok>& toks,
                              const std::string& rel,
                              std::vector<Violation>& out)
{
    const std::map<std::string, std::set<int>>& table = integer_arg_bindings();

    // function 关键词前缀计数：判定赋值与使用是否跨函数（同名不同变量）
    std::vector<int> fn_count(toks.size() + 1, 0);
    for (size_t j = 0; j < toks.size(); ++j) {
        fn_count[j + 1]
            = fn_count[j]
              + (toks[j].kind == LT_KW && toks[j].text == "function" ? 1 : 0);
    }

    std::vector<IntegerArgEvent> events;

    // ---- 赋值：local NAME = ... 与 NAME = ...（字段赋值、for
    // 控制变量除外）----
    for (size_t j = 0; j + 2 < toks.size(); ++j) {
        size_t name_idx = 0;
        if (toks[j].kind == LT_KW && toks[j].text == "local"
            && toks[j + 1].kind == LT_NAME && toks[j + 2].kind == LT_OP
            && toks[j + 2].text == "=") {
            name_idx = j + 1;
        }
        else if (toks[j].kind == LT_NAME && toks[j + 1].kind == LT_OP
                 && toks[j + 1].text == "=") {
            if (j > 0 && toks[j - 1].kind == LT_OP
                && (toks[j - 1].text == "." || toks[j - 1].text == ":")) {
                continue;  // 字段赋值
            }
            if (j > 0 && toks[j - 1].kind == LT_KW
                && (toks[j - 1].text == "local" || toks[j - 1].text == "for")) {
                continue;  // 声明分支已处理 / for 控制变量
            }
            name_idx = j;
        }
        else {
            continue;
        }
        const size_t eq      = name_idx + 1;
        const size_t rhs_end = assign_rhs_end(toks, eq);
        IntegerArgEvent e;
        e.pos       = rhs_end;
        e.kind      = 0;
        e.name      = toks[name_idx].text;
        e.float_rhs = has_float_division(toks, eq, rhs_end);
        e.decl_tok  = name_idx;
        e.decl_line = toks[name_idx].line;
        events.push_back(e);
    }

    // ---- 调用实参：直接含 '/' 的当场报，含浮点局部的进有序追踪 ----
    for (size_t k = 0; k + 1 < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME) continue;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "(") continue;

        const bool method
            = k > 0 && toks[k - 1].kind == LT_OP && toks[k - 1].text == ":";
        std::string call_name;
        if (method) {
            call_name = toks[k].text;
        }
        else if (k > 1 && toks[k - 1].kind == LT_OP && toks[k - 1].text == "."
                 && toks[k - 2].kind == LT_NAME && toks[k - 2].text == "string"
                 && toks[k].text == "format") {
            call_name = "string.format";
        }
        else if (k > 0 && toks[k - 1].kind == LT_OP
                 && toks[k - 1].text == ".") {
            continue;  // 其他表字段调用，不属绑定
        }
        else {
            call_name = toks[k].text;
        }

        std::vector<int> lpositions;
        if (call_name == "string.format") {
            // 格式串占 L1，第 n 个整型转换符对应 L(n+2) 的 vararg
            if (k + 2 >= toks.size() || toks[k + 2].kind != LT_STR) continue;
            const std::string& fmt = toks[k + 2].text;
            int ordinal            = 0;
            for (size_t p = 0; p < fmt.size(); ++p) {
                if (fmt[p] != '%') continue;
                if (p + 1 < fmt.size() && fmt[p + 1] == '%') {
                    ++p;
                    continue;
                }
                size_t q = p + 1;
                while (q < fmt.size() && strchr("-+ #0", fmt[q]) != nullptr)
                    ++q;
                while (q < fmt.size() && fmt[q] >= '0' && fmt[q] <= '9') ++q;
                if (q < fmt.size() && fmt[q] == '.') {
                    ++q;
                    while (q < fmt.size() && fmt[q] >= '0' && fmt[q] <= '9')
                        ++q;
                }
                if (q < fmt.size() && strchr("diouxX", fmt[q]) != nullptr) {
                    lpositions.push_back(ordinal + 2);
                    ++ordinal;
                }
                p = q;
            }
            if (lpositions.empty()) continue;
        }
        else {
            auto it = table.find(call_name);
            if (it == table.end()) continue;
            lpositions.assign(it->second.begin(), it->second.end());
        }

        const size_t open  = k + 1;
        const size_t close = match_paren(toks, open);
        if (close == toks.size()) continue;
        std::vector<std::pair<size_t, size_t>> args;
        split_call_args(toks, open, close, args);

        for (int lpos : lpositions) {
            if (method && lpos < 2) continue;  // L1 是 self，不是脚本实参
            const size_t arg_idx = (size_t)(lpos - 1 - (method ? 1 : 0));
            if (arg_idx >= args.size()) continue;
            const size_t ab = args[arg_idx].first;
            const size_t ae = args[arg_idx].second;
            if (ab >= ae) continue;

            if (has_float_division(toks, ab - 1, ae)) {
                Violation v;
                v.line = toks[k].line;
                v.key  = "I:" + rel + ":" + call_name;
                v.detail
                    = call_name
                      + " takes an integer argument, but '/' in the argument"
                        " yields a float that may have no integer"
                        " representation (runtime: number has no integer"
                        " representation). Use '//' or wrap it in"
                        " math.floor(...)";
                out.push_back(v);
                continue;
            }
            bool has_name = false;
            for (size_t t = ab; t < ae; ++t) {
                if (toks[t].kind == LT_NAME) {
                    has_name = true;
                    break;
                }
            }
            if (!has_name) continue;
            IntegerArgEvent e;
            e.pos       = k;
            e.kind      = 1;
            e.name      = call_name;
            e.arg_begin = ab;
            e.arg_end   = ae;
            e.line      = toks[k].line;
            e.lpos      = lpos;
            events.push_back(e);
        }
    }

    // ---- 有序追踪：赋值先于同位读取，语句右侧的读取看到赋值前的值 ----
    std::sort(events.begin(), events.end(),
              [](const IntegerArgEvent& a, const IntegerArgEvent& b) {
                  if (a.pos != b.pos) return a.pos < b.pos;
                  return a.kind < b.kind;
              });

    struct FloatLocal {
        size_t decl_tok;
        int line;
    };
    std::map<std::string, FloatLocal> floats;
    for (const IntegerArgEvent& e : events) {
        if (e.kind == 0) {
            if (e.float_rhs) {
                floats[e.name] = {e.decl_tok, e.decl_line};
            }
            else {
                floats.erase(e.name);
            }
            continue;
        }
        for (size_t t = e.arg_begin; t < e.arg_end; ++t) {
            if (toks[t].kind != LT_NAME) continue;
            auto it = floats.find(toks[t].text);
            if (it == floats.end()) continue;
            if (fn_count[e.pos] > fn_count[it->second.decl_tok]) continue;
            Violation v;
            v.line = e.line;
            v.key  = "I:" + rel + ":" + e.name;
            v.detail
                = e.name + " takes an integer argument; '" + toks[t].text
                  + "' holds a float assigned on line "
                  + std::to_string(it->second.line)
                  + " ('/' there), which may have no integer representation"
                    " (runtime: number has no integer representation). Use"
                    " '//' or math.floor(...) at the assignment";
            out.push_back(v);
            break;  // 一个实参报一次
        }
    }
}

// ======================== 防线 5：call_out 目标可见性 ========================
//
// 注册期引擎按名先查 owner 的脚本表、缺名回退当前执行脚本的表（见
// CLuaEngine::register_callout），两处都查不到即 LOG_ERROR 拒绝注册。
// 规则（docs/guide/call_out.md 规则1）：延迟函数必须是**发起 call_out 的
// 脚本**的返回表字段（含 __index 基链与别名导出）。本道只判「发起脚本的表里
// 有没有」：
//   在表里                   → 通过
//   本文件只以全局/local 定义 → 违规（两处表都查不到，注册必拒）
//   别的脚本的表里有          → NOTE（跨文件 owner 契约形态，需人工核对）
//   何处都无表字段            → 违规（注册必拒）
// 非字面量实参与 remove_call_out 配对不在本道（见文件头盲区4）。

struct CalloutCand {
    size_t file_idx = 0;
    int line        = 0;
    std::string name;
};

void detect_callout_sites(const std::vector<LexTok>& toks,
                          const std::string& rel_path, size_t file_idx,
                          const FileScan& fs, std::vector<Violation>& out,
                          std::vector<CalloutCand>& cross)
{
    for (size_t k = 0; k + 2 < toks.size(); ++k) {
        const LexTok& t = toks[k];
        if (t.kind != LT_NAME) continue;
        if (t.text != "call_out" && t.text != "call_out_combat") continue;
        if (k == 0 || toks[k - 1].kind != LT_OP
            || (toks[k - 1].text != ":" && toks[k - 1].text != "."))
            continue;  // 不是对象方法调用（裸 call_out 归防线3）
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "(") continue;
        const LexTok& arg = toks[k + 2];
        if (arg.kind != LT_STR) continue;  // 非字面量：静态不可判（盲区4）

        const std::string& name = arg.text;
        if (fs.methods.count(name)) continue;  // 规则1：表里有，通过

        if (fs.defs.count(name) || fs.locals.count(name)) {
            Violation v;
            v.line = t.line;
            v.key  = "C:" + rel_path + ":" + key_escape(name);
            v.detail = "call_out target '" + name
                       + "' is defined in this file but never exported to the "
                         "script table (local/global); registration will be "
                         "rejected at runtime (docs/guide/call_out.md rule 1)";
            out.push_back(v);
            continue;
        }
        cross.push_back({file_idx, t.line, name});
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
//  3. _check_require_path —— 与引擎 searcher 同一套 package.path 解析（见
//     CLuaEngine::init 里的 gbkSearcherLua），只回文件路径、不执行模块；
//     防线9 拿它定位模块源码，再静态取导出面。
const char* k_checker_helpers_lua
    = "function _check_require_closure(modname)\n"
      "    local ok, res = pcall(package.searchers[2], modname)\n"
      "    if ok and type(res) == 'function' then return true, '' end\n"
      "    if ok and type(res) == 'string' then return false, res end\n"
      "    return false, tostring(res)\n"
      "end\n"
      "function _check_require_path(modname)\n"
      "    local modpath = modname:gsub('%.', '/')\n"
      "    for pattern in package.path:gmatch('[^;]+') do\n"
      "        local filepath = pattern:gsub('?', modpath)\n"
      "        local f = io.open(filepath, 'rb')\n"
      "        if f then f:close() return filepath end\n"
      "    end\n"
      "    return nil\n"
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

// ======================== 防线 9：模块表字段对账 ========================
//
// 模块没导出的字段被调用，运行期抛 "attempt to call a nil value (field 'x')"
// —— 数据集 mingjiao_db 把 on_mj_special 定义成全局函数，35 处
// mj_db.on_mj_special(...) 全部取到 nil，cast 当场失败。防线4 只看"语料里
// 有没有人定义过这个名字"，全局定义的同名函数会把这类调用整片放走，故本道
// 把字面量 require 绑定的局部与**该模块自己的导出面**对账：
//   导出面可判 + 字段只被调用（全文件从未被读）→ 缺失即违规
//   导出面不可判（动态键 / 元表 / 返回值不是表）→ 整模块不报
//   局部被重新赋值或以别的方式使用 → 不再跟踪
// 只判调用：`if m.optional then m.optional() end` 这类先探测后调用的可选
// 字段是合法写法。别名转发（local a = m）、经字段保存的 require、
// m.sub.fn() 的 fn 追不到接收者，一律不判 —— 宁可漏、不可误报。

// 模块导出面：known=false 表示静态不可判定，本道对该模块一律不报
struct ModuleSurface {
    bool known = false;
    std::set<std::string> exports;
};

// 字面量 require 绑定的局部；decl 是局部名所在词元（作用域判定用）
struct ModuleBinding {
    std::string name;
    std::string module;
    size_t decl = 0;
};

// 一次接收者字段访问；call=false 是读（探测或当值传递）
struct ModuleFieldUse {
    std::string module;
    std::string field;
    int line  = 0;
    bool call = false;
};

// 语句块深度（只计语句关键字，不计括号/花括号）：function/if/for/while/do/
// repeat 开一块，end/until 收一块，for/while 头部自带的 do 不单独计数。
// pre[k] 是处理词元 k 之前的深度。块栈口径与 analyze_scope 一致（那边还要
// 判表字段，故各自维护）。
void block_depths(const std::vector<LexTok>& toks, std::vector<int>& pre)
{
    pre.assign(toks.size() + 1, 0);
    bool pending_do = false;
    for (size_t k = 0; k < toks.size(); ++k) {
        pre[k + 1]      = pre[k];
        const LexTok& t = toks[k];
        if (t.kind != LT_KW) continue;
        if (t.text == "function" || t.text == "if" || t.text == "repeat") {
            ++pre[k + 1];
        }
        else if (t.text == "for" || t.text == "while") {
            ++pre[k + 1];
            pending_do = true;
        }
        else if (t.text == "do") {
            if (pending_do)
                pending_do = false;
            else
                ++pre[k + 1];
        }
        else if ((t.text == "end" || t.text == "until") && pre[k + 1] > 0) {
            --pre[k + 1];
        }
    }
}

// 模块的返回语句：语句块深度 0 处的 return。没有、或不止一个（分支返回）
// 都判不可判，返回 toks.size()。
size_t top_level_return(const std::vector<LexTok>& toks,
                        const std::vector<int>& pre)
{
    size_t found = toks.size();
    for (size_t k = 0; k < toks.size(); ++k) {
        if (toks[k].kind != LT_KW || toks[k].text != "return") continue;
        if (pre[k] != 0) continue;
        if (found != toks.size()) return toks.size();
        found = k;
    }
    return found;
}

// open 指向 KW "function"，返回配对 end 的下标；无配对返回 toks.size()
size_t match_function_end(const std::vector<LexTok>& toks, size_t open,
                          const std::vector<int>& pre)
{
    const int body = pre[open] + 1;
    for (size_t j = open + 1; j < toks.size(); ++j) {
        if (toks[j].kind == LT_KW && toks[j].text == "end" && pre[j] == body)
            return j;
    }
    return toks.size();
}

// 表构造器顶层键（NAME = …、[ "str" ] = …）。非字面量下标键判不可判（返回
// false）；函数体整体跳过——体内的 x = … 是语句不是键；嵌套表的键不算顶层。
bool collect_ctor_keys(const std::vector<LexTok>& toks, size_t open,
                       const std::vector<int>& pre, std::set<std::string>& out)
{
    const size_t close = match_brace(toks, open);
    if (close == toks.size()) return false;
    size_t depth = 1;
    for (size_t k = open + 1; k < close; ++k) {
        const LexTok& t = toks[k];
        if (t.kind == LT_OP && t.text == "{") {
            ++depth;
            continue;
        }
        if (t.kind == LT_OP && t.text == "}") {
            --depth;
            continue;
        }
        if (depth != 1) continue;
        if (t.kind == LT_KW && t.text == "function") {
            const size_t e = match_function_end(toks, k, pre);
            if (e == toks.size() || e >= close) return false;
            k = e;
            continue;
        }
        if (t.kind == LT_OP && t.text == "[") {
            if (k + 3 >= close || toks[k + 1].kind != LT_STR
                || toks[k + 2].kind != LT_OP || toks[k + 2].text != "]"
                || toks[k + 3].kind != LT_OP || toks[k + 3].text != "=")
                return false;  // [ 表达式 ] = … 运行期才定键
            out.insert(toks[k + 1].text);
            k += 3;
            continue;
        }
        if (t.kind == LT_NAME && k + 1 < close && toks[k + 1].kind == LT_OP
            && toks[k + 1].text == "=")
            out.insert(t.text);
    }
    return true;
}

// `return NAME` 的模块：NAME 恰好被赋值一次、初值是表构造器，之后的
// `NAME . 字段` 写入即导出面。动态下标（NAME [）、元表与 rawset（可能凭空
// 添字段）都判不可判。
bool module_surface_from_var(const std::vector<LexTok>& toks,
                             const std::string& name,
                             const std::vector<int>& pre, ModuleSurface& out)
{
    out.exports.clear();
    out.known = false;

    size_t init_rhs = toks.size();
    int assigns     = 0;
    for (size_t k = 0; k + 2 < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME || toks[k].text != name) continue;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "=") continue;
        if (k > 0 && toks[k - 1].kind == LT_OP
            && (toks[k - 1].text == "." || toks[k - 1].text == ":"
                || toks[k - 1].text == "]"))
            continue;  // 字段写入，不是对模块变量的赋值
        ++assigns;
        init_rhs = k + 2;
    }
    if (assigns != 1) return false;
    if (init_rhs >= toks.size() || toks[init_rhs].kind != LT_OP
        || toks[init_rhs].text != "{")
        return false;

    for (size_t k = 0; k + 1 < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME || toks[k].text != name) continue;
        if (toks[k + 1].kind == LT_OP && toks[k + 1].text == "[") return false;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != ".") continue;
        if (k + 2 >= toks.size() || toks[k + 2].kind != LT_NAME) continue;
        const bool write = k + 3 < toks.size() && toks[k + 3].kind == LT_OP
                           && toks[k + 3].text == "=";
        const bool fn    = k > 0 && toks[k - 1].kind == LT_KW
                           && toks[k - 1].text == "function";
        if (write || fn) out.exports.insert(toks[k + 2].text);
    }
    for (size_t k = 0; k + 2 < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME) continue;
        if (toks[k].text != "setmetatable" && toks[k].text != "rawset")
            continue;
        if (k > 0 && toks[k - 1].kind == LT_OP && toks[k - 1].text == ".")
            continue;
        if (toks[k + 1].kind != LT_OP || toks[k + 1].text != "(") continue;
        if (toks[k + 2].kind == LT_NAME && toks[k + 2].text == name)
            return false;
    }

    if (!collect_ctor_keys(toks, init_rhs, pre, out.exports)) {
        out.exports.clear();
        return false;
    }
    out.known = true;
    return true;
}

// 模块导出面：返回表构造器取顶层键，`return NAME` 取 NAME 的字段写入；
// 其余形态（返回调用 / 字面量 / 分支返回 / 无返回）判不可判。
ModuleSurface analyze_module_surface(const std::vector<LexTok>& toks)
{
    ModuleSurface s;
    if (toks.empty()) return s;
    std::vector<int> pre;
    block_depths(toks, pre);
    const size_t ret = top_level_return(toks, pre);
    if (ret == toks.size() || ret + 1 >= toks.size()) return s;
    const LexTok& value = toks[ret + 1];
    if (value.kind == LT_OP && value.text == "{") {
        if (!collect_ctor_keys(toks, ret + 1, pre, s.exports))
            return ModuleSurface();
        s.known = true;
        return s;
    }
    if (value.kind == LT_NAME
        && module_surface_from_var(toks, value.text, pre, s))
        return s;
    return ModuleSurface();
}

// 按模块名取导出面（缓存）：路径解析不到、模块读不到都记为不可判——
// require 解析失败已由防线2 单独报，这里不重复。
const ModuleSurface& resolve_module_surface(
    lua_State* L, const std::string& mod,
    std::map<std::string, ModuleSurface>& cache)
{
    std::map<std::string, ModuleSurface>::const_iterator hit = cache.find(mod);
    if (hit != cache.end()) return hit->second;

    ModuleSurface s;
    lua_getglobal(L, "_check_require_path");
    lua_pushlstring(L, mod.c_str(), mod.size());
    if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
        const char* path = lua_tostring(L, -1);
        if (path && *path) {
            const std::string gbk = load_file_as_gbk(path);
            if (!gbk.empty()) {
                std::vector<LexTok> toks;
                lex_lua(gbk, toks);
                s = analyze_module_surface(toks);
            }
        }
    }
    lua_pop(L, 1);
    return cache.emplace(mod, s).first->second;
}

// 调用点绑定：`local NAME = require("lit")` 与 `local NAME = require "lit"`
void collect_module_bindings(const std::vector<LexTok>& toks,
                             std::vector<ModuleBinding>& out)
{
    for (size_t k = 0; k + 3 < toks.size(); ++k) {
        if (toks[k].kind != LT_KW || toks[k].text != "local") continue;
        if (toks[k + 1].kind != LT_NAME) continue;
        if (toks[k + 2].kind != LT_OP || toks[k + 2].text != "=") continue;
        if (toks[k + 3].kind != LT_NAME || toks[k + 3].text != "require")
            continue;
        std::string mod;
        if (k + 4 < toks.size() && toks[k + 4].kind == LT_STR) {
            mod = toks[k + 4].text;  // require "lit"：无括号写法
        }
        else if (k + 6 < toks.size() && toks[k + 4].kind == LT_OP
                 && toks[k + 4].text == "(" && toks[k + 5].kind == LT_STR
                 && toks[k + 6].kind == LT_OP && toks[k + 6].text == ")") {
            mod = toks[k + 5].text;
        }
        else {
            continue;  // 模块名非字面量：防线2 记 NOTE，本道不判
        }
        out.push_back({toks[k + 1].text, mod, k + 1});
    }
}

// 局部只在「链首字段访问」里出现才跟踪：重新赋值、当值传递、别名转发、
// 形参同名遮蔽都追不到绑定，继续跟踪只会误报。
bool binding_is_trackable(const std::vector<LexTok>& toks,
                          const ModuleBinding& b)
{
    for (size_t k = 0; k < toks.size(); ++k) {
        if (toks[k].kind != LT_NAME || toks[k].text != b.name) continue;
        if (k == b.decl) continue;
        if (k + 1 >= toks.size() || toks[k + 1].kind != LT_OP
            || (toks[k + 1].text != "." && toks[k + 1].text != ":"))
            return false;
        if (k > 0 && toks[k - 1].kind == LT_OP
            && (toks[k - 1].text == "." || toks[k - 1].text == ":"
                || toks[k - 1].text == "]"))
            return false;  // 是别人的字段，只是撞了名
    }
    return true;
}

// 接收者字段访问：NAME . 字段 / NAME : 字段，以及 require("lit") . 字段。
// 只记链首字段——m.sub.fn() 的 fn 追不到接收者，不判。
void collect_module_field_uses(const std::vector<LexTok>& toks,
                               const std::vector<ModuleBinding>& bindings,
                               std::vector<ModuleFieldUse>& out)
{
    std::map<std::string, std::string> by_name;
    for (const ModuleBinding& b : bindings) by_name[b.name] = b.module;

    for (size_t k = 1; k + 1 < toks.size(); ++k) {
        if (toks[k].kind != LT_OP
            || (toks[k].text != "." && toks[k].text != ":"))
            continue;
        const LexTok& recv = toks[k - 1];
        std::string module;
        if (recv.kind == LT_NAME) {
            std::map<std::string, std::string>::const_iterator it
                = by_name.find(recv.text);
            if (it == by_name.end()) continue;
            module = it->second;
        }
        else if (recv.kind == LT_OP && recv.text == ")" && k >= 4
                 && toks[k - 4].kind == LT_NAME && toks[k - 4].text == "require"
                 && toks[k - 3].kind == LT_OP && toks[k - 3].text == "("
                 && toks[k - 2].kind == LT_STR
                 && match_paren(toks, k - 3) == k - 1
                 && (k < 5 || toks[k - 5].kind != LT_OP
                     || (toks[k - 5].text != "." && toks[k - 5].text != ":"))) {
            module = toks[k - 2].text;  // require("lit").field 直链
        }
        else {
            continue;
        }
        if (toks[k + 1].kind != LT_NAME) continue;
        out.push_back({module, toks[k + 1].text, toks[k].line,
                       k + 2 < toks.size() && toks[k + 2].kind == LT_OP
                           && toks[k + 2].text == "("});
    }
}

// 按（模块, 字段）归并：被读过一次就当可选字段放行；只被调用的字段才与
// 导出面对账，导出面不可判或字段确实在里面则放行。
void detect_module_field_calls(const std::vector<LexTok>& toks,
                               const std::string& rel_path, lua_State* L,
                               std::map<std::string, ModuleSurface>& surfaces,
                               std::vector<Violation>& out)
{
    std::vector<ModuleBinding> bindings;
    collect_module_bindings(toks, bindings);
    std::vector<ModuleBinding> tracked;
    for (const ModuleBinding& b : bindings)
        if (binding_is_trackable(toks, b)) tracked.push_back(b);

    std::vector<ModuleFieldUse> uses;
    collect_module_field_uses(toks, tracked, uses);
    if (uses.empty()) return;

    typedef std::pair<std::string, std::string> FieldKey;  // {模块, 字段}
    std::map<FieldKey, int> calls;                         // 首次调用行号
    std::set<FieldKey> reads;
    for (const ModuleFieldUse& u : uses) {
        const FieldKey key(u.module, u.field);
        if (u.call)
            calls.emplace(key, u.line);
        else
            reads.insert(key);
    }

    for (std::map<FieldKey, int>::const_iterator it = calls.begin();
         it != calls.end(); ++it) {
        if (reads.count(it->first)) continue;  // 被探测过 → 可选字段
        const ModuleSurface& s
            = resolve_module_surface(L, it->first.first, surfaces);
        if (!s.known || s.exports.count(it->first.second)) continue;
        Violation v;
        v.line = it->second;
        v.key  = "F:" + rel_path + ":" + key_escape(it->first.first) + ":"
                 + key_escape(it->first.second);
        v.detail = "module field '" + it->first.second
                   + "' is not exported by require(\"" + it->first.first
                   + "\"); calling it raises \"attempt to call a nil value"
                     " (field '" + it->first.second + "')\" at runtime";
        out.push_back(v);
    }
}

// ======================== 每文件结果 ========================

struct FileOutcome {
    bool readable  = true;
    bool syntax_ok = false;
    std::string syntax_err;
    std::string require_err;  // 非空 = 闭包解析失败
    int dynamic_count    = 0;
    int callout_contract = 0;  // 防线5：目标在别的脚本表里（NOTE，不判失败）
    std::vector<Violation> violations;  // 新增（未被基线豁免）
    int baselined = 0;
};

// 待全量 script_defs 收集完成后消解的候选
struct Cand {
    size_t file_idx = 0;
    int line        = 0;
    std::string name;
    bool method = false;
    // 接收者是 self：self 恒为引擎压入的对象 userdata，解析口径与其它
    // 接收者（可能是脚本表）不同，见 pass 2 的分流。
    bool self_recv = false;
    // 引用落在 `local X = { ... }` 自己的构造器里：X 此刻还没有作用域，
    // 读到的是全局（nil）——pass 2 直接判违规，不走 all_defs 消解。
    bool self_init = false;
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

// 毫秒墙钟：TIME 行按阶段拆分耗时，慢在哪一段一眼可见
static long long elapsed_ms(std::chrono::steady_clock::time_point since)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - since)
        .count();
}

int check_lua_files(lua_State* L, const std::vector<std::string>& files,
                    const CheckLuaOptions* opts)
{
    if (!L) return 1;

    const auto t_start = std::chrono::steady_clock::now();

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
    // 仅对象元表的键：self 上唯一可能存在的方法集合（见 pass 2 分流）
    std::set<std::string> m_object;
    collect_globals(L, g_whitelist);
    collect_object_methods(L, m_static);
    collect_object_methods(L, m_object);
    collect_stdlib_methods(L, m_static);
    collect_file_methods(L, m_static);

    const char* path_prefix = opts ? opts->path_prefix : nullptr;

    std::vector<FileOutcome> outcomes(files.size());
    std::vector<std::string> rel_paths(files.size());
    std::vector<Cand> cands;
    std::vector<CalloutCand> callout_cross;  // 防线5：待全量表定义消解的站点
    std::map<std::string, ModuleSurface>
        module_surfaces;  // 防线9：模块导出面缓存
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

        // 防线7：random 语义（0 基下标 / 非整型实参 / math.random 与 0 基混用）
        detect_random_misuse(toks, files[i], rel_paths[i], fs, o.violations);

        // 防线8：整型实参绑定的 '/' 实参（直接与经局部变量的间接形态）
        detect_integer_arg_float(toks, rel_paths[i], o.violations);

        // 防线5：call_out 注册名在本文件的判定（跨文件的入候选，pass2 消解）
        detect_callout_sites(toks, rel_paths[i], i, fs, o.violations,
                             callout_cross);

        // 防线9：字面量 require 绑定的局部与其模块导出面的对账
        detect_module_field_calls(toks, rel_paths[i], L, module_surfaces,
                                  o.violations);

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
                // 接收者是 `:` 前的那个 token；`foo():bar()` 没有名字，
                // 按普通接收者处理。
                const bool self_recv = k >= 2 && toks[k - 2].kind == LT_NAME
                                       && toks[k - 2].text == "self";
                if (self_recv) {
                    cands.push_back({i, t.line, t.text, true, true, false});
                    continue;
                }
                const bool self_init = inside_own_init(fs, t.text, k);
                if (!self_init) {
                    if (fs.locals.count(t.text)) continue;
                    if (m_static.count(t.text)) continue;
                }
                cands.push_back({i, t.line, t.text, true, false, self_init});
                continue;
            }

            if (prev && prev->kind == LT_OP
                && (prev->text == "." || prev->text == ":"))
                continue;  // 字段名，不是变量读

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

            // `local X = { ... X.y ... }`：X 在自己的构造器里还没有作用域，
            // 这里读到的是全局（运行期为 nil）。无条件上报，不看 locals /
            // 白名单 / all_defs——同名全局存在也只是串到别人的表上。
            // （写入形态已在上面按 assign_lhs 跳过：`local a = { a = 1 }` 里
            //  的 `a = 1` 是表字段定义，不是变量读。）
            if (inside_own_init(fs, t.text, k)) {
                cands.push_back({i, t.line, t.text, false, false, true});
                continue;
            }

            if (fs.locals.count(t.text)) continue;
            if (g_whitelist.count(t.text)) continue;
            cands.push_back({i, t.line, t.text, false, false, false});
        }
    }

    const auto t_scan = std::chrono::steady_clock::now();

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
        if (c.self_init) {
            // 作用域规则：局部变量从声明语句之后才可见，`local X = { ... }`
            // 构造器里的 X 必然读到全局（nil 或串台到别人的表）——不消解、
            // 不豁免，直接判违规。
            Violation v;
            v.line = c.line;
            v.key  = "I:" + c.name;
            v.detail = "local '" + c.name
                       + "' is used inside its own initializer (local "
                       + c.name + " = {...}); its scope starts after the "
                         "declaration, so this reads the global";
            outcomes[c.file_idx].violations.push_back(v);
            continue;
        }
        if (c.method) {
            if (c.self_recv) {
                // self 是引擎压入的对象 userdata（lua_bind_push_object →
                // jianghu_object 元表），脚本表字段在它上面不可达：按名在
                // 「某个脚本里定义过」放行只会放走必炸的调用，故只认元表键。
                if (m_object.count(c.name)) continue;
            }
            else if (all_methods.count(c.name))
                continue;  // 任一脚本已定义
            Violation v;
            v.line = c.line;
            v.key  = "M:" + c.name;
            v.detail = c.self_recv
                           ? "method '" + c.name
                                 + "' is not on the object metatable; self is "
                                   "the engine-pushed object, so script-table "
                                   "functions cannot resolve through it"
                           : "method '" + c.name
                                 + "' is not bound on the object metatable, not "
                                   "a stdlib ':'-call method, and not defined "
                                   "by any script";
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

    // ---- 防线 5 的跨文件候选：目标在别的脚本表里 → NOTE（owner 契约形态，
    // 需人工核对 owner 的脚本确实定义它）；任何表里都没有 → 违规 ----
    for (const CalloutCand& c : callout_cross) {
        if (all_methods.count(c.name)) {
            ++outcomes[c.file_idx].callout_contract;
            continue;
        }
        Violation v;
        v.line = c.line;
        v.key  = "C:" + rel_paths[c.file_idx] + ":" + key_escape(c.name);
        v.detail = "call_out target '" + c.name
                   + "' is not a field of any script table; registration will "
                     "be rejected at runtime (docs/guide/call_out.md rule 1)";
        outcomes[c.file_idx].violations.push_back(v);
    }

    // ---- 防线 6：钩子冒烟（默认开，--no-smoke 关）----
    // 排在基线之前：冒烟翻出的存量错误同样走基线豁免，门禁只拦新增。
    SmokeReport smoke;
    long long smoke_ms = 0;
    if (!opts || opts->smoke) {
        const auto t_smoke = std::chrono::steady_clock::now();
        smoke              = smoke_check_items(L, files);
        smoke_ms           = elapsed_ms(t_smoke);

        std::map<std::string, size_t> by_path;
        for (size_t i = 0; i < files.size(); ++i) by_path[files[i]] = i;
        for (const SmokeViolation& sv : smoke.violations) {
            std::map<std::string, size_t>::const_iterator it
                = by_path.find(sv.file);
            if (it == by_path.end()) continue;
            Violation v;
            v.line   = 0;  // 冒烟错误没有源码行号，错误信息自带现场
            v.key    = "S:" + sv.hook;
            v.detail = sv.hook + ": " + sv.message;
            outcomes[it->second].violations.push_back(v);
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

    // resolve = 定义补充扫描 + pass2 + 基线过滤；冒烟夹在中间，单独扣掉
    const long long resolve_ms = elapsed_ms(t_scan) - smoke_ms;
    const auto t_output        = std::chrono::steady_clock::now();

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
                    if (v.line > 0)
                        printf("  line %d: %s\n", v.line, v.detail.c_str());
                    else
                        printf("  %s\n", v.detail.c_str());  // 冒烟类，无行号
                }
            }
            if (o.dynamic_count > 0) {
                printf(
                    "NOTE: %s: %d dynamic require(...) not statically "
                    "checked\n",
                    files[i].c_str(), o.dynamic_count);
            }
            if (o.callout_contract > 0) {
                printf(
                    "NOTE: %s: %d call_out target(s) live in another "
                    "file's table (owner-contract pattern - confirm the "
                    "owner's script defines it)\n",
                    files[i].c_str(), o.callout_contract);
            }
            if (o.baselined > 0) {
                printf("NOTE: %s: %d pre-existing violation(s) baselined\n",
                       files[i].c_str(), o.baselined);
            }
            ++failed;
            // 门禁可能被数据打进崩溃：逐条落盘，崩在哪个文件当场可见
            fflush(stdout);
            continue;
        }
        if (o.dynamic_count > 0) {
            printf("NOTE: %s: %d dynamic require(...) not statically checked\n",
                   files[i].c_str(), o.dynamic_count);
        }
        if (o.callout_contract > 0) {
            printf(
                "NOTE: %s: %d call_out target(s) live in another file's "
                "table (owner-contract pattern - confirm the owner's "
                "script defines it)\n",
                files[i].c_str(), o.callout_contract);
        }
        if (o.baselined > 0) {
            printf("NOTE: %s: %d pre-existing violation(s) baselined\n",
                   files[i].c_str(), o.baselined);
        }
        printf("OK:   %s\n", files[i].c_str());
        ++success;
        fflush(stdout);  // 同上：崩溃时能定位到最后一个完成的文件
    }

    const long long output_ms = elapsed_ms(t_output);

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

    // ---- 耗时：按阶段拆开，慢在哪一段一眼可见 ----
    const long long total_ms = elapsed_ms(t_start);
    const long long scan_ms  = total_ms - elapsed_ms(t_scan);
    printf(
        "\nTIME: total %lld ms | scan %lld | resolve %lld | smoke %lld"
        " | output %lld\n",
        total_ms, scan_ms, resolve_ms, smoke_ms, output_ms);
    if (!opts || opts->smoke) {
        printf(
            "TIME: smoke: %d script(s) drove %d hook(s), %d skipped,"
            " slowest %lld ms (%s)\n",
            smoke.scripts, smoke.hooks, smoke.skipped, smoke.slowest_ms,
            smoke.slowest_script.c_str());
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

    std::string configPath = "config.ini";
    std::string dirPath;
    std::string baselinePath;
    bool writeBaseline = false;
    bool smoke         = true;  // 防线6 默认开
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
        else if (strncmp(p, "--no-smoke", 10) == 0) {
            p += 10;
            smoke = false;
        }
        else if (strncmp(p, "--smoke", 7) == 0) {
            p += 7;  // 兼容写法：冒烟本就是默认开
        }
        else {
            files.push_back(extract_arg(p));
        }
    }

    if (dirPath.empty() && files.empty()) {
        printf(
            "usage: jianghu.exe --check-lua [--config <ini>] [--dir <path>]\n"
            "                    [--baseline <file>] [--write-baseline]\n"
            "                    [--no-smoke]\n"
            "                    <file.lua> ...\n"
            "  hook smoke (create/do_use) is ON by default; --no-smoke\n"
            "  turns it off. Hook errors, errors swallowed by script pcall,\n"
            "  and module-body errors become violations.\n");
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
    opts.smoke          = smoke;
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

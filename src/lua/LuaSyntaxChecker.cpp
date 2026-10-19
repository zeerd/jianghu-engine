/** @file LuaSyntaxChecker.cpp @brief Lua 脚本语法校验实现 通过
 * CLuaEngine::Init() 初始化完整的 Lua 绑定环境， 然后对每个指定文件做
 * luaL_loadbuffer 语法校验。 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
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

// 提取一个参数值（到下一个空白或字符串结尾）
static std::string extract_arg(const char*& p)
{
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

// ======================== require 闭包校验 ========================
// 运行期 require("a.b") 失败会直接使脚本加载报错（历史事故：
// lua-default/skill/high/animal/*.lua 引用了不存在的 skill.skill_utils，
// 语法校验通过、运行才炸）。这里在 --check-lua 阶段把字面量 require 的
// 目标文件提前解析出来，把该类问题挡在启动之前。

static bool is_lua_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_lua_ident(char c)
{
    return is_lua_ident_start(c) || (c >= '0' && c <= '9');
}

// 若 s[i] 处是长括号开括号（[[ 或 [=[ …），经 open_len 返回开括号长度
// （2 + 等号数），并返回对应闭括号串（"]]"、"]=]" 等）；否则返回空串。
static std::string long_bracket(const std::string& s, size_t i,
                                size_t& open_len)
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

// 从脚本源码收集字面量 require("mod") 的模块名。
// 跳过行注释（--…）、长注释（--[[…]]）、短字符串（"…" '…'）、长字符串（[[…]]），
// 因此注释/字符串里出现的 require 示例不会被误收集。
// 非字面量调用（require(name) 等）计入 dynamic_count，无法静态校验。
// 源码可为 GBK 或 UTF-8：扫描只针对 ASCII 结构字符，多字节内容按字节顺序
// 跳过（多字节字节均 >= 0x80，不会与 ASCII 结构符混淆）。
static void collect_require_calls(const std::string& src,
                                  std::vector<std::string>& out,
                                  int& dynamic_count)
{
    dynamic_count  = 0;
    const size_t n = src.size();
    size_t i       = 0;
    while (i < n) {
        const char c = src[i];

        // 注释：-- … 或 --[[ … ]]
        if (c == '-' && i + 1 < n && src[i + 1] == '-') {
            size_t open_len              = 0;
            std::string close            = long_bracket(src, i + 2, open_len);
            if (!close.empty()) {
                size_t e = src.find(close, i + 2 + open_len);
                i        = (e == std::string::npos) ? n : e + close.size();
            }
            else {
                size_t e = src.find('\n', i + 2);
                i        = (e == std::string::npos) ? n : e;
            }
            continue;
        }

        // 短字符串
        if (c == '"' || c == '\'') {
            size_t j = i + 1;
            while (j < n && src[j] != c) {
                if (src[j] == '\\') ++j;
                ++j;
            }
            i = (j < n) ? j + 1 : n;
            continue;
        }

        // 长字符串
        if (c == '[') {
            size_t open_len   = 0;
            std::string close = long_bracket(src, i, open_len);
            if (!close.empty()) {
                size_t e = src.find(close, i + open_len);
                i        = (e == std::string::npos) ? n : e + close.size();
                continue;
            }
        }

        // 标识符
        if (is_lua_ident_start(c)) {
            size_t j = i;
            while (j < n && is_lua_ident(src[j])) ++j;
            const std::string ident = src.substr(i, j - i);
            i                       = j;
            if (ident == "require") {
                // 跳过空白找 '('，再跳过空白看是否为字面量模块名
                size_t k = i;
                while (k < n
                       && (src[k] == ' ' || src[k] == '\t' || src[k] == '\r'
                           || src[k] == '\n'))
                    ++k;
                if (k < n && src[k] == '(') {
                    ++k;
                    while (k < n
                           && (src[k] == ' ' || src[k] == '\t' || src[k] == '\r'
                               || src[k] == '\n'))
                        ++k;
                    if (k < n && (src[k] == '"' || src[k] == '\'')) {
                        const char q = src[k];
                        size_t e     = k + 1;
                        while (e < n && src[e] != q) {
                            if (src[e] == '\\') ++e;
                            ++e;
                        }
                        if (e < n) {
                            out.push_back(src.substr(k + 1, e - k - 1));
                            i = e + 1;
                        }
                        continue;
                    }
                    size_t open_len   = 0;
                    std::string close = long_bracket(src, k, open_len);
                    if (!close.empty()) {
                        size_t e = src.find(close, k + open_len);
                        if (e != std::string::npos) {
                            out.push_back(
                                src.substr(k + open_len, e - (k + open_len)));
                            i = e + close.size();
                            continue;
                        }
                    }
                    ++dynamic_count;  // require(非字面量)：静态无法校验
                }
            }
            continue;
        }

        ++i;
    }
}

// require 闭包解析助手（安装到 Lua 全局）：复用引擎 searcher 的
// package.path 解析口径，只查找与编译、不执行模块。
// 返回 (true, "") 或 (false, 解析失败原因)。
static const char* k_require_closure_lua =
    "function _check_require_closure(modname)\n"
    "    local ok, res = pcall(package.searchers[2], modname)\n"
    "    if ok and type(res) == 'function' then return true, '' end\n"
    "    if ok and type(res) == 'string' then return false, res end\n"
    "    return false, tostring(res)\n"
    "end\n";

// 校验单个文件源码里的字面量 require 闭包；打印失败项，全部可解析返回 true。
static bool check_require_closure(lua_State* L, const std::string& filepath,
                                  const std::string& gbk_source)
{
    std::vector<std::string> modules;
    int dynamic_count = 0;
    collect_require_calls(gbk_source, modules, dynamic_count);

    bool ok = true;
    for (size_t i = 0; i < modules.size(); ++i) {
        lua_getglobal(L, "_check_require_closure");
        lua_pushlstring(L, modules[i].c_str(), modules[i].size());
        if (lua_pcall(L, 1, 2, 0) != LUA_OK) {
            printf("FAIL: %s\n  require check error: %s\n", filepath.c_str(),
                   lua_tostring(L, -1));
            lua_pop(L, 1);
            ok = false;
            continue;
        }
        if (!lua_toboolean(L, -2)) {
            const char* why = lua_tostring(L, -1);
            printf("FAIL: %s\n  require(\"%s\") cannot resolve: %s\n",
                   filepath.c_str(), modules[i].c_str(), why ? why : "");
            ok = false;
        }
        lua_pop(L, 2);
    }
    if (dynamic_count > 0) {
        printf("NOTE: %s: %d dynamic require(...) not statically checked\n",
               filepath.c_str(), dynamic_count);
    }
    return ok;
}

int check_lua_files(lua_State* L, const std::vector<std::string>& files)
{
    if (!L) return 1;

    if (luaL_dostring(L, k_require_closure_lua) != LUA_OK) {
        printf("ERROR: cannot install require-closure checker: %s\n",
               lua_tostring(L, -1));
        lua_pop(L, 1);
        return (int)files.size();
    }

    int failed  = 0;
    int success = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        const std::string& filepath = files[i];

        std::string gbk = load_file_as_gbk(filepath.c_str());
        if (gbk.empty()) {
            printf("FAIL: %s\n  cannot read file\n", filepath.c_str());
            ++failed;
            continue;
        }
        int rc = luaL_loadbuffer(L, gbk.c_str(), gbk.size(), filepath.c_str());
        if (rc != LUA_OK) {
            printf("FAIL: %s\n  %s\n", filepath.c_str(), lua_tostring(L, -1));
            lua_pop(L, 1);
            ++failed;
            continue;
        }
        lua_pop(L, 1);  // 语法通过，丢弃编译后的 chunk

        if (check_require_closure(L, filepath, gbk)) {
            printf("OK:   %s\n", filepath.c_str());
            ++success;
        }
        else {
            ++failed;
        }
    }

    printf("\n%d file(s) OK, %d file(s) FAILED\n", success, failed);
    return failed;
}

int run_lua_checker(const char* cmdLine)
{
    // 1. 解析参数
    const char* p = strstr(cmdLine, "--check-lua");
    if (!p) {
        printf("ERROR: --check-lua flag found but cannot parse\n");
        return 1;
    }
    p += strlen("--check-lua");
    p = skip_arg(p);

    // 可选: --config <path>
    const char* configPath = "data/config.ini";  // 默认使用服务器配置
    if (strncmp(p, "--config", 8) == 0) {
        p += 8;
        p                 = skip_arg(p);
        configPath        = p;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        std::string cfg(start, p - start);
        configPath = _strdup(cfg.c_str());
        p          = skip_arg(p);
    }

    // 可选: --dir <path>（递归扫描目录下所有 .lua 文件）
    std::string dirPath;
    if (strncmp(p, "--dir", 5) == 0) {
        p += 5;
        p                 = skip_arg(p);
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        dirPath = std::string(start, p - start);
        p       = skip_arg(p);
    }

    if (dirPath.empty() && !*p) {
        printf(
            "usage: jianghu.exe --check-lua [--config <ini>] [--dir <path>] "
            "<file.lua> ...\n");
        return 1;
    }

    // 2. 加载配置
    ini_load(configPath);

    // 3. 初始化 Lua 引擎 + 绑定
    CLuaEngine* engine      = CLuaEngine::instance();
    const char* luaRoot     = ini_get("lua", "script_root", "lua");
    const char* defaultRoot = ini_get("lua", "default_script_root", "");
    if (!engine->init(luaRoot, defaultRoot)) {
        printf("ERROR: Lua engine init failed\n");
        return 1;
    }
    lua_State* L = engine->get_state();

    // 4. 收集文件列表
    std::vector<std::string> files;

    if (!dirPath.empty()) {
        collect_lua_files(dirPath, files);
        printf("Found %d .lua files in %s\n", (int)files.size(),
               dirPath.c_str());
    }

    while (*p) {
        p = skip_arg(p);
        if (!*p) break;
        files.push_back(extract_arg(p));
    }

    // 5. 逐个校验（语法 + require 闭包）
    int failed = check_lua_files(L, files);

    // 6. 清理
    engine->shutdown();

    return failed ? 1 : 0;
}

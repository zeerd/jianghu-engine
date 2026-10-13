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

    // 5. 逐个校验语法
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
        }
        else {
            printf("OK:   %s\n", filepath.c_str());
            lua_pop(L, 1);
            ++success;
        }
    }

    // 6. 清理
    engine->shutdown();

    printf("\n%d file(s) OK, %d file(s) FAILED\n", success, failed);
    return failed ? 1 : 0;
}

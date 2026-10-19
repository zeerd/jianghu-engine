#include "config.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "compat.h"

static std::string trim(const std::string &s)
{
    auto start = s.find_first_not_of(" \t\r");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r");
    return s.substr(start, end - start + 1);
}

static std::string default_config_path()
{
    std::string home = home_dir();
    if (home.empty()) return "./jianghu.conf";
    return home + "/.jianghu/config";
}

static std::string expand_home(const std::string &path)
{
    if (path.empty() || path[0] != '~') return path;
    std::string home = home_dir();
    return home.empty() ? path : home + path.substr(1);
}

static void ensure_dir(const std::string &path)
{
    make_dirs(path);
}

std::string config_path()
{
    // Allow override via CC_JIANGHU_CONFIG env var
    const char *env = getenv("CC_JIANGHU_CONFIG");
    if (env && strlen(env) > 0) return std::string(env);
    return default_config_path();
}

Config load_config(const std::string &path)
{
    Config cfg;
    std::string p = path.empty() ? default_config_path() : path;
    std::ifstream f(p);
    if (!f.is_open()) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "host")
            cfg.host = val;
        else if (key == "port")
            cfg.port = std::stoi(val);
        else if (key == "account")
            cfg.account = val;
        else if (key == "password")
            cfg.password = val;
        else if (key == "logs_dir")
            cfg.logs_dir = val;
        else if (key == "heartbeat_interval")
            cfg.heartbeat_interval = std::stoi(val);
    }

    // Expand ~ in paths
    cfg.logs_dir = expand_home(cfg.logs_dir);

    // Set defaults if empty
    if (cfg.logs_dir.empty()) {
        cfg.logs_dir = expand_home("~/.local/share/jianghu/logs");
    }

    // Ensure directories exist
    ensure_dir(cfg.logs_dir);

    return cfg;
}

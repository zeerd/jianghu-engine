#pragma once

#include <string>

struct Config {
    std::string host = "127.0.0.1";
    int port         = 6600;
    std::string account;
    std::string password;
    std::string logs_dir;   // Directory for log files, default  ~/.local/share/jianghu/logs
    int heartbeat_interval = 15;  // heartbeat keep-alive interval in
                                  // seconds, 0 = disabled
    // 通讯帧 XOR 密钥：留空用内置默认（服务端 [security] comm_key 为空时
    // 同样回退默认）。服务端与 CLI 必须写成同一份，否则互相解不开帧。
    std::string comm_key;
};

// Return the path to  ~/.local/share/jianghu/config or fall back to ./jianghu.conf
std::string config_path();

// Load configuration from file. Returns Config with defaults for any missing
// keys.
Config load_config(const std::string &path = "");

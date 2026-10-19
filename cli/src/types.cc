#include "types.h"

std::string GameMessage::get(const std::string &key,
                             const std::string &default_val) const
{
    auto it = params.find(key);
    return (it != params.end()) ? it->second : default_val;
}

int64_t GameMessage::get_int(const std::string &key, int64_t default_val) const
{
    auto it = params.find(key);
    if (it == params.end()) return default_val;
    try {
        return std::stoll(it->second);
    } catch (...) {
        return default_val;
    }
}

bool GameMessage::has(const std::string &key) const
{
    return params.find(key) != params.end();
}

std::string GameMessage::to_string() const
{
    std::string out = "GameMessage(cmd=" + std::to_string(cmd) + ", params={";
    bool first      = true;
    for (const auto &kv : params) {
        if (!first) out += ", ";
        first = false;
        out += kv.first + "=" + kv.second;
    }
    out += "})";
    return out;
}

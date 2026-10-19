#include "color.h"

#include <map>
#include <regex>

namespace color {

static const std::map<std::string, std::string> color_map = {
    // Basic colors
    {"$GND", "\033[0;30m"},
    {"$BLK", "\033[0;30m"},
    {"$RED", "\033[31m"},
    {"$YEL", "\033[33m"},
    {"$GRN", "\033[32m"},
    {"$BLU", "\033[34m"},
    {"$CYN", "\033[36m"},
    {"$MAG", "\033[35m"},
    {"$WHT", "\033[37m"},
    // Highlight colors
    {"$HIR", "\033[1;31m"},
    {"$HIG", "\033[1;32m"},
    {"$HIY", "\033[1;33m"},
    {"$HIB", "\033[1;34m"},
    {"$HIC", "\033[1;36m"},
    {"$HIM", "\033[1;35m"},
    {"$HIW", "\033[1;37m"},
    // Common color (dim)
    {"$COM", "\033[0m"},
};

std::string convert(const std::string &text)
{
    std::string result;
    result.reserve(text.size() + 32);

    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '$' && i + 3 < text.size()) {
            // Check for 3-char code: $RED, $CYN, etc.
            std::string code = text.substr(i, 4);
            auto it          = color_map.find(code);
            if (it != color_map.end()) {
                result += it->second;
                i += 4;
                continue;
            }
            // Check single char variable ($N, $n, $D)
            if (text[i + 1] == 'N' || text[i + 1] == 'n'
                || text[i + 1] == 'D') {
                i += 2;
                continue;
            }
        }
        result += text[i];
        ++i;
    }
    return result;
}

std::string strip(const std::string &text)
{
    std::string result;
    result.reserve(text.size());

    for (size_t i = 0, n = text.size(); i < n; ++i) {
        if (text[i] == '$' && i + 4 <= n) {
            // Check for known color code: $XXX in color_map
            std::string code = text.substr(i, 4);
            if (color_map.find(code) != color_map.end()) {
                // Skip adjacent codes too ($$RED etc.)
                size_t end = i;
                while (end + 4 <= n && text[end] == '$') {
                    if (color_map.find(text.substr(end, 4))
                        != color_map.end()) {
                        end += 4;
                    }
                    else {
                        break;
                    }
                }
                i = end - 1;
                continue;
            }
        }
        result += text[i];
    }

    return result;
}

}  // namespace color

#pragma once

#include <string>

namespace color {

// Convert service-side color codes ($RED, $HIR, etc.) to ANSI escape sequences
std::string convert(const std::string &text);

// Remove service-side color codes ($RED, $HIR, etc.) from the given string
std::string strip(const std::string &text);

}  // namespace color
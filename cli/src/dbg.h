#pragma once

#include <atomic>
#include <iostream>

// Global debug log enable/disable control.
// Enabled via dbg_enable(true) from main.cc when --dbg flag is present.
extern std::atomic<bool> g_dbg_enabled;

inline void dbg_enable(bool enabled)
{
    g_dbg_enabled.store(enabled, std::memory_order_relaxed);
}

// DBG(x) — conditionally print debug log to stderr.
// When disabled (default), compiles away with zero overhead.
#define DBG(x)                                               \
    do {                                                     \
        if (g_dbg_enabled.load(std::memory_order_relaxed)) { \
            std::cerr << "[DBG] " << x << std::endl;         \
        }                                                    \
    } while (0)

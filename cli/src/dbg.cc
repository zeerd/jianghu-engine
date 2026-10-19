#include "dbg.h"

#include <fstream>
#include <mutex>

std::atomic<bool> g_dbg_enabled{false};
std::ofstream g_dbg_log_file;
std::mutex g_dbg_log_mutex;
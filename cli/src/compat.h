#pragma once

// 平台兼容层：Windows 与 POSIX 的差异集中在此文件。
// 其余源码通过这里的封装处理平台差异，尽量不直接写 #ifdef。

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// winsock2.h 必须先于 windows.h，避免宏冲突
#include <windows.h>

// Winsock 未定义 MSG_NOSIGNAL / SHUT_RDWR
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif

using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
/// getsockname/getpeername 的地址长度参数类型
using socket_len_t = int;

/// 进程内初始化一次 Winsock，之后调用无副作用。
inline bool net_init()
{
    static const bool ok = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ok;
}

inline bool set_nonblocking(socket_t s, bool on)
{
    u_long mode = on ? 1u : 0u;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}

inline void close_socket(socket_t s) { closesocket(s); }

inline int net_last_error() { return WSAGetLastError(); }

inline bool net_would_block() { return net_last_error() == WSAEWOULDBLOCK; }

inline std::string net_error_message()
{
    return "socket error " + std::to_string(net_last_error());
}

inline bool net_interrupted() { return false; }

/// 把控制台输入/输出代码页设为 UTF-8，使程序内部统一的 UTF-8
/// 字符串能正确显示与读取（否则中文在默认 GBK 控制台下会乱码）。
inline void init_console()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

#else  // ---- POSIX ----

#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <arpa/inet.h>

using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
/// getsockname/getpeername 的地址长度参数类型
using socket_len_t = socklen_t;

inline bool net_init() { return true; }

inline bool set_nonblocking(socket_t s, bool on)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(s, F_SETFL, flags) == 0;
}

inline void close_socket(socket_t s) { ::close(s); }

inline int net_last_error() { return errno; }

inline bool net_would_block()
{
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

inline std::string net_error_message() { return std::strerror(errno); }

inline bool net_interrupted() { return errno == EINTR; }

/// POSIX 控制台本就使用 UTF-8（或由终端处理编码），无需初始化。
inline void init_console() {}

#endif

/// 用户主目录（Windows 用 USERPROFILE，其余用 HOME），不存在时返回空串。
inline std::string home_dir()
{
#ifdef _WIN32
    const char *home = std::getenv("USERPROFILE");
#else
    const char *home = std::getenv("HOME");
#endif
    return home ? std::string(home) : std::string();
}

/// 递归创建目录（等价于 mkdir -p），返回是否成功。
inline bool make_dirs(const std::string &path)
{
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec;
}

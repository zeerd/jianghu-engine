/**
 * @file main.cpp
 * @brief Standard Win32 entry point */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Global.h"
#include "ServerSockHandler.h"
#include "Logger.h"
#include "MainConfig.h"
#include "util/CommandLine.h"
#include "util/IniParser.h"
#include "resource.h"

// Lua syntax checker mode
#include "LuaSyntaxChecker.h"

// External declarations
extern DWORD WINAPI main_worker_thread(LPVOID lpParameter);
extern bool g_reboot;

// Definition for g_ini_data (declared in IniParser.h)
std::map<std::string, std::map<std::string, std::string>> g_ini_data;

// Window class name
static const char* g_szWindowClass = "JHServer";

// Global window handle
static HWND g_hWnd = NULL;

// 实际使用的配置文件路径（WinMain 解析后保存，供窗口过程显示）
static char g_config_path[1024] = "data/config.ini";

// 信息面板子控件 ID
#define IDC_INFO_VERSION 1001
#define IDC_INFO_CONFIG  1002
#define IDC_INFO_PORT    1003

// 创建并排版「版本号 / 配置文件 / 端口号」信息面板
static void create_info_panel(HWND hWnd, HINSTANCE hInstance)
{
    char buf[1200];
    int x = 12, y = 12, w = 360, h = 18, gap = 6;
    // STATIC 默认用旧位图 System 字体，视觉偏粗；改用标准 GUI
    // 字体去掉粗体观感。
    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    HWND item = CreateWindowEx(
        0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h, hWnd,
        (HMENU)(UINT_PTR)IDC_INFO_VERSION, hInstance, NULL);
    snprintf(buf, sizeof(buf), "版本号:   %s", MAIN_VERSION);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowText(item, buf);
    y += h + gap;

    item = CreateWindowEx(
        0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS, x,
        y, w, h, hWnd, (HMENU)(UINT_PTR)IDC_INFO_CONFIG, hInstance, NULL);
    snprintf(buf, sizeof(buf), "配置文件: %s", g_config_path);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowText(item, buf);
    y += h + gap;

    item = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT, x,
                          y, w, h, hWnd, (HMENU)(UINT_PTR)IDC_INFO_PORT,
                          hInstance, NULL);
    snprintf(buf, sizeof(buf), "端口号:   %d", g_server_port);
    SendMessage(item, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowText(item, buf);
    y += h + gap;
}

// Forward declaration
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// ---------------- 系统托盘 ----------------
// 最小化后窗口从任务栏隐藏，仅驻留系统托盘：双击托盘图标恢复窗口，
// 右键弹出「打开 / 退出」菜单。

// 托盘回调消息（lParam 为鼠标消息：WM_LBUTTONDBLCLK / WM_RBUTTONUP 等）
#define WM_TRAYICON (WM_APP + 1)
// 托盘图标 ID（同进程可有多个图标，用 uID 区分）
#define TRAY_ICON_ID 1

// 托盘图标是否已添加（避免重复 NIM_ADD / NIM_DELETE）
static bool g_tray_added = false;

// 应用图标：优先取资源里的 mud.ico（IDR_MAINFRAME），资源缺失时退回系统
// 默认图标——绝不能以 NULL 图标注册托盘（那样会是个透明的空图标）。
static HICON load_app_icon()
{
    HICON icon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MAINFRAME));
    if (!icon) {
        icon = LoadIcon(NULL, IDI_APPLICATION);
    }
    return icon;
}

// 添加托盘图标（窗口创建成功后调用）
static void tray_icon_add(HWND hWnd)
{
    NOTIFYICONDATA nid = {0};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = hWnd;
    nid.uID              = TRAY_ICON_ID;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = load_app_icon();
    // 提示文字复用窗口标题（如 "JH Server - GS20261028-xxx"）
    GetWindowTextA(hWnd, nid.szTip, sizeof(nid.szTip));
    g_tray_added = Shell_NotifyIcon(NIM_ADD, &nid) != FALSE;
}

// 移除托盘图标（窗口销毁时调用）
static void tray_icon_remove()
{
    if (!g_tray_added) {
        return;
    }
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_hWnd;
    nid.uID    = TRAY_ICON_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
    g_tray_added = false;
}

// 从托盘恢复窗口（托盘双击 / 菜单「打开」）
static void tray_icon_restore(HWND hWnd)
{
    // 隐藏前窗口处于最小化态，必须用 SW_RESTORE 才能还原为正常大小
    ShowWindow(hWnd, SW_RESTORE);
    SetForegroundWindow(hWnd);
}

// 托盘右键菜单：打开（默认项）/ 退出
static void tray_icon_menu(HWND hWnd)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenu(menu, MF_STRING, 1, "打开(&O)");
    AppendMenu(menu, MF_STRING, 2, "退出(&X)");
    SetMenuDefaultItem(menu, 1, FALSE);

    POINT pt = {0};
    GetCursorPos(&pt);
    // TrackPopupMenu 需要前台窗口，否则点击菜单外区域后菜单不消失
    SetForegroundWindow(hWnd);
    UINT cmd = (UINT)TrackPopupMenu(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0,
        hWnd, NULL);
    DestroyMenu(menu);
    // 配套上面的前台窗口技巧：补一条 WM_NULL 让菜单正确收起
    PostMessage(hWnd, WM_NULL, 0, 0);

    if (cmd == 1) {
        tray_icon_restore(hWnd);
    } else if (cmd == 2) {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
}

// WinMain entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)nCmdShow;

    // --- 检查 --check-lua 模式 ---
    if (strstr(lpCmdLine, "--check-lua")) {
        return run_lua_checker(lpCmdLine);
    }

    // Initialize Winsock
    WSADATA wsd;
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
        MessageBox(NULL, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 读取配置：默认 data/config.ini，可用 --config <path> 覆盖
    // 注意：parse_config_path 仅在命令行真有 --config 时才写入 out 缓冲；
    // 无 --config 时直接返回 default_path 而不碰缓冲，故必须用其返回值。
    char config_path[1024] = "";
    const char* resolved_config
        = parse_config_path(lpCmdLine, config_path, sizeof(config_path));
    ini_load(resolved_config);
    // 保存实际配置路径，供窗口信息面板显示
    snprintf(g_config_path, sizeof(g_config_path), "%s", resolved_config);
    char dbip[80], user[80], pwd[80];
    strncpy(dbip, ini_get("database", "host", "127.0.0.1"), sizeof(dbip) - 1);
    strncpy(user, ini_get("database", "user", "root"), sizeof(user) - 1);
    strncpy(pwd, ini_get("database", "password", "root"), sizeof(pwd) - 1);
    dbip[sizeof(dbip) - 1] = '\0';
    user[sizeof(user) - 1] = '\0';
    pwd[sizeof(pwd) - 1]   = '\0';

    // Initialize game
    if (!init_game(dbip, user, pwd)) {
        MessageBox(NULL, "Init_Game failed", "Error", MB_OK | MB_ICONERROR);
        WSACleanup();
        return 1;
    }

    // Register window class
    WNDCLASSEX wcex    = {0};
    wcex.cbSize        = sizeof(WNDCLASSEX);
    wcex.style         = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc   = WndProc;
    wcex.cbClsExtra    = 0;
    wcex.cbWndExtra    = 0;
    wcex.hInstance     = hInstance;
    wcex.hIcon         = load_app_icon();
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm       = load_app_icon();

    if (!RegisterClassEx(&wcex)) {
        WSACleanup();
        MessageBox(NULL, "RegisterClassEx failed", "Error",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create window
    char window_title[256];
    snprintf(window_title, sizeof(window_title), "JH Server - %s",
             MAIN_VERSION);
    g_hWnd = CreateWindowEx(
        0, g_szWindowClass, window_title, WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInstance, NULL);

    if (!g_hWnd) {
        DWORD err = GetLastError();
        char errMsg[256];
        sprintf(errMsg, "CreateWindowEx failed: %lu", err);
        WSACleanup();
        MessageBox(NULL, errMsg, "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Show minimized
    ShowWindow(g_hWnd, SW_SHOWMINIMIZED);
    UpdateWindow(g_hWnd);

    // Message loop
    MSG msg;
    BOOL ret;
    while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (ret == -1) {
            DWORD err = GetLastError();
            char errMsg[256];
            sprintf(errMsg, "GetMessage failed: %lu", err);
            MessageBox(NULL, errMsg, "Error", MB_OK | MB_ICONERROR);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    WSACleanup();
    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
        case WM_CREATE: {
            // Initialize server socket
            if (!init_server_socket(g_ssock, &g_saddr, 0, g_server_port, 1)) {
                MessageBox(hWnd, "Failed to create server socket", "Error",
                           MB_OK | MB_ICONERROR);
                return -1;
            }

            // Start worker thread
            if (!init_thread(main_worker_thread)) {
                MessageBox(hWnd, "Failed to create worker thread", "Error",
                           MB_OK | MB_ICONERROR);
                return -1;
            }

            // Set timer for heartbeat (1 second)
            SetTimer(hWnd, 1, 1000, NULL);

            // 创建信息面板（版本号 / 配置文件 / 端口号）
            create_info_panel(
                hWnd, (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE));

            // 添加系统托盘图标
            tray_icon_add(hWnd);
            break;
        }

        case WM_SYSCOMMAND: {
            // 最小化 → 隐藏窗口（任务栏按钮消失），仅保留托盘图标；
            // 双击托盘图标恢复。其余系统命令照常处理（须掩码 0xFFF0，
            // 低位可能带 SC_KEYMENU 等附加信息）。
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        case WM_TRAYICON: {
            switch ((UINT)lParam) {
                case WM_LBUTTONDBLCLK:
                    tray_icon_restore(hWnd);
                    break;
                case WM_RBUTTONUP:
                    tray_icon_menu(hWnd);
                    break;
            }
            break;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                if (g_reboot) {
                    PostQuitMessage(0);
                }
            }
            break;
        }

        case WM_DESTROY: {
            // Cleanup
            tray_icon_remove();
            KillTimer(hWnd, 1);
            clear_socket(g_ssock);
            g_terminated = true;

            // Signal IOCP worker threads to exit
            signal_iocp_threads_exit();

            // Wait for all worker threads to exit (including MainWorkerThread
            // and IOCP threads)
            wait_for_all_worker_threads(10000);

            PostQuitMessage(0);
            break;
        }

        case WM_CLOSE: {
            // Allow close to trigger WM_DESTROY
            DestroyWindow(hWnd);
            break;
        }

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

/**
 * @file main.cpp
 * @brief Standard Win32 entry point */

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Global.h"
#include "ServerSockHandler.h"
#include "Logger.h"
#include "MainConfig.h"
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

// Forward declaration
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

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

    // Read configuration from data/config.ini
    ini_load("data/config.ini");
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
    wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));
    wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName  = NULL;
    wcex.lpszClassName = g_szWindowClass;
    wcex.hIconSm       = LoadIcon(hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));

    if (!RegisterClassEx(&wcex)) {
        WSACleanup();
        MessageBox(NULL, "RegisterClassEx failed", "Error",
                   MB_OK | MB_ICONERROR);
        return 1;
    }

    // Create window
    g_hWnd = CreateWindowEx(
        0, g_szWindowClass, "JH Server", WS_OVERLAPPEDWINDOW | WS_MINIMIZE,
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

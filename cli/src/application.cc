#include "application.h"

#include <iostream>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include "color.h"
#include "dbg.h"

Application::Application(const Config& cfg) : core_(cfg) {}

Application::~Application()
{
    // 清理由 run() 负责。
}

int Application::run()
{
    DBG("Built at " << __DATE__ " " __TIME__);

    // 初始化：连接 + 自动登录 + 后台线程
    core_.start();

    // 主循环
    main_loop();

    // 清理
    core_.stop();
    return 0;
}

void Application::save_terminal()
{
#ifdef _WIN32
    // Windows 控制台默认即为行输入+回显，无需设置。
#else
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        t.c_lflag |= ICANON | ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
#endif
}

void Application::main_loop()
{
    save_terminal();

    auto& input = core_.input();
    std::string line;
    std::cout << "> ";
    while (core_.running() && std::getline(std::cin, line)) {
        bool keep = input.process_line(line);
        if (!keep || input.should_quit()) {
            core_.request_stop();
            core_.ui().shutdown();
        }
        core_.process_received();
        std::cout << "> ";
    }
    restore_terminal();
}

void Application::restore_terminal()
{
#ifdef _WIN32
    // Windows 控制台默认即为行输入+回显，无需恢复。
#else
    struct termios t;
    if (tcgetattr(STDIN_FILENO, &t) == 0) {
        t.c_lflag |= ICANON | ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }
#endif
}

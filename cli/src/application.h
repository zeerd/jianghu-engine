#pragma once

#include "client_core.h"
#include "config.h"

/// 交互式客户端：在 ClientCore 之上提供终端主循环。
class Application {
public:
    explicit Application(const Config& cfg);
    ~Application();
    int run();

private:
    void save_terminal();
    void main_loop();
    void restore_terminal();

    ClientCore core_;
};

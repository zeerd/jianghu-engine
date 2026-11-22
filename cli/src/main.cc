#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "application.h"
#include "bot.h"
#include "client_core.h"
#include "compat.h"
#include "config.h"
#include "dbg.h"
#include "scenario.h"

namespace {

struct CliOptions {
    Config cfg;
    bool show_help = false;
    int exit_code  = 0;
    std::string script;
    std::string report;
    std::string vars;
    int step_timeout_ms = 15000;
    bool bot            = false;  // 压测 bot 模式（随机行动循环）
    std::string account;          // 覆盖配置里的登录账号（bot 用）
    std::string password;         // 覆盖配置里的登录密码（bot 用）
};

void print_help(const char* program)
{
    std::cout
        << "Usage: " << program
        << " [-h] [--dbg] [--config file] [--script file]\n"
           "\n"
           "  -h, --help                 显示帮助\n"
           "  -d, --dbg                  打开调试输出\n"
           "  -c, --config <file>        指定配置文件\n"
           "      --script <file>        无头剧本模式（不进入交互）\n"
           "      --report <file>        剧本结果报告输出路径\n"
           "      --vars <file>          剧本变量文件（key=value）\n"
           "      --step-timeout <ms>    单个 wait/assert 的超时（默认 "
           "15000）\n"
           "      --bot                  压测 bot 模式：登录后随机行动\n"
           "      --account <id>         覆盖配置里的登录账号（bot 用）\n"
           "      --password <pw>        覆盖配置里的登录密码（bot 用）\n";
}

/// 解析命令行参数。返回 0 继续运行，1 应直接退出。
int parse_args(int argc, char* argv[], CliOptions& opt)
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto need_value
            = [&](const std::string& name, std::string& out) -> bool {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                opt.exit_code = 1;
                return false;
            }
            out = argv[++i];
            return true;
        };

        if (arg == "-h" || arg == "--help") {
            opt.show_help = true;
            opt.exit_code = 0;
            return 1;
        }
        if (arg == "-d" || arg == "--dbg") {
            dbg_enable(true);
            continue;
        }
        if (arg == "-c" || arg == "--config") {
            std::string path;
            if (!need_value(arg, path)) return 1;
            opt.cfg = load_config(path);
            continue;
        }
        if (arg.rfind("--config=", 0) == 0) {
            opt.cfg = load_config(arg.substr(9));
            continue;
        }
        if (arg == "--script") {
            if (!need_value(arg, opt.script)) return 1;
            continue;
        }
        if (arg.rfind("--script=", 0) == 0) {
            opt.script = arg.substr(9);
            continue;
        }
        if (arg == "--report") {
            if (!need_value(arg, opt.report)) return 1;
            continue;
        }
        if (arg.rfind("--report=", 0) == 0) {
            opt.report = arg.substr(9);
            continue;
        }
        if (arg == "--vars") {
            if (!need_value(arg, opt.vars)) return 1;
            continue;
        }
        if (arg.rfind("--vars=", 0) == 0) {
            opt.vars = arg.substr(7);
            continue;
        }
        if (arg == "--step-timeout") {
            std::string v;
            if (!need_value(arg, v)) return 1;
            try {
                opt.step_timeout_ms = std::stoi(v);
            } catch (...) {
                std::cerr << "Invalid --step-timeout: " << v << "\n";
                opt.exit_code = 1;
                return 1;
            }
            continue;
        }
        if (arg == "--bot") {
            opt.bot = true;
            continue;
        }
        if (arg == "--account") {
            if (!need_value(arg, opt.account)) return 1;
            continue;
        }
        if (arg == "--password") {
            if (!need_value(arg, opt.password)) return 1;
            continue;
        }
        if (arg.rfind("--step-timeout=", 0) == 0) {
            try {
                opt.step_timeout_ms = std::stoi(arg.substr(15));
            } catch (...) {
                std::cerr << "Invalid --step-timeout: " << arg << "\n";
                opt.exit_code = 1;
                return 1;
            }
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            opt.exit_code = 1;
            return 1;
        }
        std::cout << "Positional arg: " << arg << "\n";
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    init_console();

    CliOptions opt;
    opt.cfg = load_config();

    if (parse_args(argc, argv, opt)) {
        if (opt.show_help) print_help(argv[0]);
        return opt.exit_code;
    }
    // 命令行凭据覆盖配置文件（bot 一进程一账号，不为每个账号写 conf）
    if (!opt.account.empty()) opt.cfg.account = opt.account;
    if (!opt.password.empty()) opt.cfg.password = opt.password;
    DBG("Built at " << __DATE__ " " __TIME__);

    try {
        if (opt.bot) {
            // 压测 bot 模式：登录失败(2)不重试——凭据/网络问题重连也没用；
            // 断线(3)退避后重建 ClientCore 重连，直到外部杀进程。
            for (int attempt = 0;; ++attempt) {
                int rc = 0;
                try {
                    ClientCore core(opt.cfg);
                    core.start();
                    rc = run_bot(core);
                    core.stop();
                } catch (const std::runtime_error&) {
                    rc = 3;  // 连接失败按断线处理，走退避重连
                }
                if (rc != 3) return rc;
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (attempt >= 20) return 3;  // 长时间连不上放弃
            }
        }

        if (!opt.script.empty()) {
            // 无头剧本模式
            ClientCore core(opt.cfg);
            core.start();

            ScenarioOptions sopts;
            sopts.script_path        = opt.script;
            sopts.report_path        = opt.report;
            sopts.vars_path          = opt.vars;
            sopts.default_timeout_ms = opt.step_timeout_ms;

            int rc = run_scenario(core, sopts);
            core.stop();
            return rc;
        }

        // 交互模式
        Application app(opt.cfg);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}

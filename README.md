# jianghu-engine

「金剑江湖」游戏服务端引擎：C++ 核心驱动 + Lua 游戏内容。

C++ 实现核心驱动（网络、战斗、属性、持久化等），具体游戏内容
（NPC、物品、技能、房间、任务、状态、命令）全部由 Lua 脚本实现。

## 特性

- TCP 文本协议（端口 6600，GBK 编码，XOR 混淆，`\t` 分隔），
  规范见 [docs/protocol.md](docs/protocol.md)
- Lua 5.5 脚本引擎；脚本按类型分目录加载（`npc` / `item` /
  `skill` / `room` / `condition` / `data`），支持热重载
- MySQL / SQLite 双数据库后端
- 内置 `--check-lua` 模式：启动前全量校验脚本语法与绑定环境

## 目录结构

| 目录 | 说明 |
|------|------|
| `src/` | 引擎源码（char / cmd / combat / config / db / engine / interfaces / item / lua / net / npc / object / room / skill / user / util） |
| `cli/` | 终端客户端（C++17；Linux 为独立 CMake 工程，Windows 并入本解决方案） |
| `tests/unit/` | Google Test 单元测试（`test_*.cpp`）与 mock 层 |
| `tests/integration/` | 集成测试：`scenarios/`（CLI 无头剧本）、专用服务器配置 `config.ini` |
| `lua-default/` | 服务器自带默认脚本（可被用户目录同名脚本覆盖） |
| `lua-sample/` | 自包含示例数据集，用于快速体验与联调 |
| `data/config.ini` | 服务器配置（端口、数据库、脚本目录等） |
| `docs/` | [protocol.md](docs/protocol.md)（通讯协议规范）、[handle_action.md](docs/handle_action.md)（命令拦截钩子） |
| `3rd/` | 第三方依赖：lua55、mysql、sqlite3，详见 [3rd/README.md](3rd/README.md) |

## 构建

仅支持 **Win32**（x64 版本暂不可用）。使用 Visual Studio 2026 打开
`jianghu.sln`，或命令行：

```cmd
msbuild jianghu.sln /p:Configuration=Release /p:Platform=Win32
```

解决方案包含六个工程：

引擎：

- `jianghu` — 服务端可执行程序，输出到 `bin\jianghu.exe`
- `jianghu_core` — 核心静态库（服务器与测试共用）
- `tests` — 引擎单元测试程序

CLI 客户端（详见 [CLI 客户端](#cli-客户端)）：

- `jianghu_cli` — 客户端可执行程序，输出到 `bin\cli\jianghu_cli.exe`
- `jianghu_cli_lib` — 客户端核心静态库
- `jianghu_cli_tests` — 客户端单元测试程序

运行需要 `bin\libmySQL.dll`（MySQL 客户端运行库）。

## 运行

`jianghu.exe` 是 GUI 程序（WinMain 入口），启动前编辑
`data/config.ini`：

```ini
[server]
port = 6600
name = 金剑江湖

[lua]
script_root = C:\path\to\jianghu-dataset
default_script_root = C:\jianghu-engine\lua-default
```

`script_root` 指向游戏数据集；`default_script_root` 是引擎自带的
兜底脚本目录，同名脚本以用户目录（`script_root`）优先。

### 脚本语法校验

```cmd
jianghu.exe --check-lua [--config <ini>] [--dir <path>] <file.lua> ...
```

该模式会初始化完整的 Lua + C++ 绑定环境，能识别脚本中使用的
枚举、常量与绑定函数。注意：`jianghu.exe` 是 GUI 程序，命令行
调用时需将输出重定向到文件再查看。

## CLI 客户端

`cli/` 是「金剑江湖」终端客户端（C++17），连接本引擎服务端
（默认 `127.0.0.1:6600`）。

- 彩色终端界面（服务端颜色码 `$RED` 等自动转 ANSI）
- 游戏状态跟踪：房间、出口、战斗标记、属性、技能、物品、在线
- 菜单 / 对话应答（`C=27` QUESTION 消息的多级菜单输入）
- 自动登录（配置了账号密码时）、15 秒心跳保活
- 双层命令：`/xxx` 发送服务端命令，`.xxx` 本地命令

### 构建

Windows 并入本解决方案（`jianghu_cli_lib` / `jianghu_cli` /
`jianghu_cli_tests`，产物在 `bin\cli\`）。

Linux（POSIX）下是独立的 CMake 工程：

```sh
cd cli
./build.sh              # 格式化 + 构建 + 运行测试
./build.sh --coverage   # Clang + llvm-cov 覆盖率报告
./build.sh --clean      # 先清理构建目录
```

通用方式：`cd cli && cmake -B build && cmake --build build -j`。
Linux 产物：可执行文件 `jianghu`、静态库 `jianghu_lib`、测试
`test_cases`。平台差异集中在 `cli/src/compat.h`（socket 类型、
控制台编码、主目录、建目录等）；GBK↔UTF-8 在 Windows 走 Win32
API，无需 iconv。

### 运行

```
jianghu_cli.exe [-h] [--dbg] [--config file]   # Windows，bin\cli\
./jianghu       [-h] [--dbg] [--config file]   # Linux
```

配置文件按顺序查找：`--config` 指定 → `$CC_JIANGHU_CONFIG` →
`~/.local/share/jianghu/config` → `./jianghu.conf`。格式为
`key = value`，`;` 或 `#` 开头为注释：

```ini
host = 127.0.0.1
port = 6600
account = id
password = secret
logs_dir = ~/.local/share/jianghu/logs
```

- `host` / `port` — 服务端地址与端口（默认 `127.0.0.1:6600`）
- `account` / `password` — 登录凭据；非空时连接后自动登录
- `logs_dir` — 会话日志目录（默认 `~/.local/share/jianghu/logs`），
  按天追加写入 `YYYY-MM-DD.log`，目录不存在时自动创建

配置了 `account` + `password` 时连接后自动登录；否则进游戏后用
`/login <账号> <密码>` 登录。

### 使用

- `.help` — 查看全部命令；`.quit` / `.exit` — 退出
- `.status` `.info` `.inventory` `.skills` — 角色面板
- `.look` `.objects` `.chars` `.doors` — 场景查看
- `/t <id|name>` — 设定目标，`/t 0` 清除；`.tune [名称]` — 查看 / 切换频道（本地）
- `/look` `/get` `/give` `/say` `/tell` `/kill` `/flee` `/buy` `/sell`
  `/balance` … — 服务端命令（详见 `.help`）
- 直接输入文本 = 说话（say）

### 协议

TCP 文本协议：`\t` 分隔、GBK 编码、XOR 混淆，`&C=N&<键>=<值>&`
消息格式，规范见 [docs/protocol.md](docs/protocol.md)。抓包解密可用
[`cli/tools/wireshark-ccjh.lua`](cli/tools/wireshark-ccjh.lua)。

## 测试

Google Test；引擎测试源码放在 `tests/unit/`（`test_<功能>.cpp`），
CLI 测试源码放在 `cli/tests/`（`<功能>_test.cc`）。
集成测试（CLI 无头剧本 + 真实服务器 + OpenCppCoverage）入口在仓库根的
`run-integration-tests.ps1`，剧本与专用配置在 `tests/integration/`。

`run-coverage.ps1` 是唯一认可的单元测试编译与运行方式：它分别构建并运行
引擎测试（`Release\tests.exe`）与 CLI 测试
（`bin\cli\jianghu_cli_tests.exe`），再以 `--input_coverage` 合并两套
覆盖率，生成统一的报告与增量（Release|Win32 生产同构配置 +
OpenCppCoverage）。Debug 构建的引擎测试内置 `TestConfigGate`，运行
即报错——Debug 结果不作验收依据。

```powershell
.\run-coverage.ps1                    # 行覆盖率
.\run-coverage.ps1 -CoverageType branch
```

集成测试一键运行（需本地 MySQL 服务 "MySQLa" 与 OpenCppCoverage）：
```powershell
.\run-integration-tests.ps1
```

覆盖率历史与增量对比见 `coverage_history/`。

## 代码质量

```powershell
.\run-cppcheck.ps1    # 静态检查，结果写入 bin\cppcheck.result.txt
.\style.ps1           # clang-format + stylua 格式化，移除 Lua BOM
```

辅助工具：

- StyLua: https://github.com/JohnnyMorganz/StyLua
- OpenCppCoverage: https://github.com/OpenCppCoverage/OpenCppCoverage

## License

MIT，见 [LICENSE](LICENSE)。

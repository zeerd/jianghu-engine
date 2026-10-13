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
| `tests/` | Google Test 单元测试（`test_*.cpp`）与 mock 层 |
| `lua-default/` | 服务器自带默认脚本（可被用户目录同名脚本覆盖） |
| `lua-sample/` | 自包含示例数据集，用于快速体验与联调 |
| `data/config.ini` | 服务器配置（端口、数据库、脚本目录等） |
| `docs/` | [protocol.md](docs/protocol.md)（通讯协议规范）、[handle_action.md](docs/handle_action.md)（命令拦截钩子） |
| `3rd/` | 第三方依赖：lua55、mysql、sqlite3，详见 [3rd/README.md](3rd/README.md) |

## 构建

仅支持 **Win32**（x64 版本暂不可用）。使用 Visual Studio 打开
`jianghu.sln`，或命令行：

```cmd
msbuild jianghu.sln /p:Configuration=Release /p:Platform=Win32
```

解决方案包含三个工程：

- `jianghu` — 服务端可执行程序，输出到 `bin\jianghu.exe`
- `jianghu_core` — 核心静态库（服务器与测试共用）
- `tests` — 单元测试程序

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

## 测试

Google Test；测试源码放在 `tests/`，命名 `test_<功能>.cpp`。

`run-coverage.ps1` 是唯一认可的编译与运行方式（Release|Win32
生产同构配置 + OpenCppCoverage 覆盖率采集）。Debug 构建的测试
程序内置 `TestConfigGate`，运行即报错——Debug 结果不作验收依据。

```powershell
.\run-coverage.ps1                    # 行覆盖率
.\run-coverage.ps1 -CoverageType branch
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

## 相关仓库

- [jianghu-cli](https://github.com/zeerd/jianghu-cli) — 终端客户端

## License

MIT，见 [LICENSE](LICENSE)。

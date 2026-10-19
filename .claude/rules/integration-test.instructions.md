---
name: 'Integration Test'
description: 'Integration test guidelines for E2E scenario testing with OpenCppCoverage'
applyTo: '**/tests/integration/**'
---

# 集成测试指令文件

本文档定义了 `jianghu-engine` 项目的集成测试（E2E 端到端测试）规范、架构、运行方式与覆盖率要求。

---

## 1. 架构概览

集成测试采用 **真实服务器 + 无头 CLI 客户端 + 剧本驱动** 架构：

```
┌─────────────────────────────────────────────────────────────────┐
│                    run-integration-tests.ps1                    │
│  (构建 → OpenCppCoverage 起服 → 跑剧本 → 优雅关服 → 导出报告)   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
       ┌────────────┐  ┌────────────┐  ┌────────────┐
       │  服务器    │  │  CLI 客户端 │  │  MySQL     │
       │ jianghu.exe│  │jianghu_cli │  │ game_e2e_  │
       │ (Release)  │  │  (Release) │  │  sample    │
       └────────────┘  └────────────┘  └────────────┘
              │               │
              ▼               ▼
       ┌────────────────────────────────┐
       │      tests/integration/        │
       │  config.ini  (专用服务器配置)   │
       │  cli.conf    (CLI 连接配置)     │
       │  scenarios/  (剧本文件 *.sce)   │
       └────────────────────────────────┘
```

### 关键组件

| 组件 | 说明 |
|------|------|
| **入口脚本** | `run-integration-tests.ps1`（仓库根目录） |
| **服务器配置** | `tests/integration/config.ini` — 独立测试库、独立端口、lua-sample 脚本根 |
| **CLI 配置** | `tests/integration/cli.conf` — 连接 `127.0.0.1:6601` |
| **剧本目录** | `tests/integration/scenarios/` — 四部剧本依次跑同一角色 |
| **覆盖率工具** | OpenCppCoverage（`--optimized_build`，Release PDB 行映射） |
| **报告产出** | `coverage_report_e2e_sample/` (HTML + Cobertura)、`coverage_history/e2e_sample.cov` |

---

## 2. 目录结构

```
tests/
└── integration/             # 集成测试（E2E）
    ├── config.ini           # 专用服务器配置
    ├── cli.conf             # CLI 连接配置
    └── scenarios/           # 剧本文件
        ├── register.sce
        ├── sample_mainline.sce
        ├── sample_social.sce
        └── sample_house.sce
```

---

## 3. 运行方式

### 3.1 标准运行（完整流程含覆盖率）

```powershell
# 仓库根目录执行
.\run-integration-tests.ps1
```

**脚本会自动完成**：
1. MSBuild `Release|Win32` 构建 `jianghu.vcxproj` + `cli/jianghu_cli.vcxproj`
2. OpenCppCoverage 启动 `bin\jianghu.exe --config tests\integration\config.ini`
3. 等待端口 6601 就绪（最长 180 秒，首次 PDB 加载较慢）
4. 依次运行四部剧本：`register.sce` → `sample_mainline.sce` → `sample_social.sce` → `sample_house.sce`
5. 向服务器主窗口发 `WM_CLOSE` 优雅关服
6. OpenCppCoverage 导出覆盖率报告
7. 控制台输出：测试结果、行覆盖率、未命中函数清单（超 60 个只报数字，完整清单写入文件）

### 3.2 常用参数

| 参数 | 说明 |
|------|------|
| `-SkipBuild` | 跳过 MSBuild，直接使用现有 `bin\jianghu.exe` 与 CLI |
| `-NoCoverage` | 跳过 OpenCppCoverage，快速调试剧本（不出覆盖率报告） |
| `-ResetDb` | 运行前重建测试库（需 `mysql.exe` 在 PATH） |
| `-CliExe <path>` | 指定 CLI 可执行文件路径 |
| `-StepTimeout <ms>` | 单步 wait/assert 超时（默认 20000） |
| `-ServerReadyTimeoutSec <sec>` | 服务器就绪等待上限（默认 180） |

### 3.3 调试单个剧本

```powershell
# 仅跑主线剧本，不采集覆盖率
.\run-integration-tests.ps1 -NoCoverage -SkipBuild
# 然后手动运行 CLI：
bin\cli\jianghu_cli.exe --script tests\integration\scenarios\sample_mainline.sce --vars tests\integration\vars.txt --report report.txt
```

---

## 4. 剧本 DSL 规范

剧本文件（`*.sce`）采用行式文本，最小动词集：

| 动词 | 语法 | 说明 |
|------|------|------|
| `login` | `login <acct> <pass>` | 覆盖配置账号登录 |
| `cmd` | `cmd <line>` | 完全等同用户键入（经 `InputProcessor`） |
| `wait` | `wait <cond> [timeout=ms]` | 等待条件满足，超时即失败 |
| `menu` | `menu <序号\|文本>` | 按文本解析后选择菜单项 |
| `menuinput` | `menuinput <text>` | 喂 `$2` 占位输入 |
| `capture` | `capture <正则> as <var>` | 从最近输出抓取变量（如注册返回账号 id） |
| `assert` | `assert <cond>` | 失败即整案失败 |
| `sleep` | `sleep <ms>` | 节流 / 等心跳 |
| `include` | `include <file>` | 复用其他剧本文件 |
| `#` | `# 注释` | 行注释 |

### 4.1 wait 条件类型

| 条件 | 说明 |
|------|------|
| `always` | 立即通过 |
| `login` | 登录成功（收到欢迎/大厅） |
| `room <名\|id>` | 进入指定房间 |
| `menu` | 出现菜单 |
| `dialog` | 出现对话框 |
| `text <子串\|或…>` | 输出中包含指定文本（支持 `\|` 分隔多选） |
| `notext <子串\|或…>` | **反向监视**：出现即失败，到期无出现才通过 |
| `object <名>` | 房间内出现指定对象 |
| `item <名>` | 背包/身上出现指定物品 |
| `battle` / `battle_end` | 进入/结束战斗 |
| `attr <名> <op> <值>` | 属性断言（如 `attr hp > 100`） |

### 4.2 变量机制

- **输入变量**：由 `run-integration-tests.ps1` 写入 `--vars` 文件（如 `name=xxx` `pass=yyy` `acct=123`）
- **产出变量**：`capture` 抓取的变量供后续剧本使用
- **引用语法**：`$name`、`$acct`、`$pass` 等

---

## 5. 四部剧本职责

| 剧本 | 步数 | 核心覆盖能力 | 产出变量 |
|------|------|--------------|----------|
| `register.sce` | ~28 | 注册分步流程（姓名/密码/邮箱/性别/天赋/确认） | `acct` |
| `sample_mainline.sce` | ~200 | 登录/look/向导问答/开箱/买卖/装备/茶棚/拜师/学艺/读书/升级/睡觉/战斗/随机树箱/副本 | — |
| `sample_social.sce` | ~150 | 打坐/观察人物/毒药补药/传送符/钱庄/频道切换/表情 | — |
| `sample_house.sce` | ~80 | 申请居民/选房/起名/描述/缴银/搬进/房主操作/卖房 | — |

**执行顺序固定**：四部剧本**依次跑同一角色**，`register.sce` 产出的 `acct` 供后三部使用。

---

## 6. 服务器配置要点（`config.ini`）

```ini
[database]
name = game_e2e_sample    ; 独立测试库，服务端自动建库建表

[server]
port = 6601               ; 独立端口，避免与开发服务冲突
start_room = 青云山山脚    ; lua-sample 起始房间
rate_limit = 0            ; 关闭指令限流，避免边界随机丢弃导致 wait 超时

[lua]
script_root = lua-sample          ; 被测样本
default_script_root = lua-default ; 兜底默认脚本
```

**关键原则**：服务端通过命令行 `--config tests\integration\config.ini` 直接加载，**不再临时改写 `data/config.ini`**。

---

## 7. 覆盖率参考

> **覆盖率只是一个方便的用来确认功能是否被覆盖的手段。** 这是因为恰巧 C++ 支持这么个技术，所以我们拿来用了。它不是铁律。毕竟我们没办法拿到 Lua 覆盖率。真正用来确认功能覆盖度的永远是设计和代码自身，不是覆盖率数字。

### 7.1 采集配置

```powershell
# OpenCppCoverage 关键参数（run-integration-tests.ps1 内部）
--modules bin\jianghu.exe          # 被测模块
--sources src\*                    # 仅统计 src/
--optimized_build                  # 必加：生产是 Release /O2
--working_dir <仓库根>             # 让服务器读到相对路径配置
--export_type html:coverage_report_e2e_sample
--export_type cobertura:coverage_report_e2e_sample\coverage.xml
--export_type binary:coverage_history\e2e_sample.cov
```

### 7.2 覆盖率参考指标（非硬性门槛）

| 指标 | 说明 |
|------|------|
| **行覆盖率** | 参考上一次提交（`coverage_history/summary-e2e-sample.txt` 的 `delta`），大幅下降需排查原因 |
| **新增代码** | 应有对应剧本步骤覆盖，以确认功能可跑通 |
| **修改代码** | 应确保修改分支被剧本触达，以确认改动生效 |
| **报告附带** | 提交时附上覆盖率报告截图或关键数据，供参考 |

### 7.3 覆盖率缺口认知（来自 PoC 验证）

> **E2E 覆盖率约 33%（单测 94%），缺口是系统性的，不是操作姿势问题。**
>
> E2E **无法覆盖**的典型功能（需单测兜底）：
> - 巫师命令（`CmdWizard*.cpp`）
> - 组队/夫妻/互教/互给（多客户端）
> - 副本管理、住宅系统（部分）
> - 物品命令（`get/give/use/drop/wield` 等边界分支）
> - 持久化/数据库层（`Sql*.cpp`、`SqliteDb.cpp`）
> - 心跳/连接管理（单测已覆盖）

---

## 8. 编写/维护剧本的规范

### 8.1 新增剧本步骤

1. **先在手工交互模式验证**流程可跑通
2. **记录每一步的服务端输出**（`wait text`/`wait menu` 依赖精确文本）
3. **添加 `sleep` 节流**（网络/心跳/数据库延迟），建议 500-1000ms
4. **设置合理 `timeout`**（默认 20s，复杂交互可加大）
5. **使用 `capture` 抓取动态值**（账号 id、物品 id 等）

### 8.2 修改现有剧本

- **修改服务端输出文本** → 必须同步更新剧本中的 `wait text`/`menu` 文本
- **修改交互流程**（增删菜单层级、改命令语法） → 必须同步更新剧本步骤
- **新增 lua-sample 功能** → 必须在对应剧本中添加覆盖步骤

### 8.3 禁止事项

- ❌ 在剧本中硬编码动态值（账号 id、物品 id、随机房间名）
- ❌ 省略 `wait` 直接 `cmd`（竞态导致不稳定）
- ❌ 使用 `sleep` 代替 `wait`（除非纯节流）
- ❌ 剧本之间有隐性依赖（除显式变量传递外）
- ❌ 在 `lua-sample` 目录下放测试脚手架（剧本、配置、脚本一律在 `tests/integration/`）
- ❌ 样本(lua-sample)是教材，不是测试夹具。禁止为了测试目的在sample中添加异常逻辑。

---

## 9. 常见故障与排查

| 症状 | 常见原因 | 排查步骤 |
|------|----------|----------|
| `wait text` 超时 | 服务端输出文本变更、编码问题、限流丢弃指令 | 1. 查服务端日志 `log/` 2. 确认 `rate_limit=0` 3. 对比实际输出与剧本期望文本 |
| `wait menu` 超时 | 菜单未弹出、菜单项文本变更 | 1. 手工复现 2. 检查 Lua 脚本 `do_talk`/`menu` 分支 |
| `login` 失败 | 数据库未就绪、账号冲突、密码错误 | 1. 检查 MySQL 服务 2. `-ResetDb` 重建 3. 确认 `vars.txt` 变量正确 |
| 服务器启动超时（180s） | PDB 加载慢、端口被占用、配置错误 | 1. `netstat -ano \| findstr 6601` 2. 检查 `config.ini` 路径 3. 手工起服看报错 |
| 覆盖率报告为空/异常 | 未加 `--optimized_build`、PDB 与 exe 不匹配、源码路径错 | 1. 确认同一次构建的 exe+pdb 2. `--sources src\*` 3. 重新构建 |
| CLI 连不上 | 端口错、host 错、服务器未就绪 | 1. 检查 `cli.conf` 2. 确认服务器日志有 `Listen on 6601` |

---

## 10. 变更检查清单（提交前自查）

- [ ] 修改了服务端输出文本 → 同步更新 `tests/integration/scenarios/*.sce` 中的 `wait text`/`menu`
- [ ] 修改了交互流程/命令语法 → 同步更新剧本步骤
- [ ] 新增了 lua-sample 功能 → 在对应剧本添加覆盖步骤
- [ ] 运行 `.\run-integration-tests.ps1` 全绿
- [ ] 覆盖率 `delta ≥ 0`（对比 `coverage_history/summary-e2e-sample.txt`）
- [ ] 无新增「未命中函数」属于本次修改范围（或有合理解释）
- [ ] 没有在代码中保留注释形式的迁移记录

---

> **核心原则**：集成测试验证"真实服务器能否跑通真实业务流程"。覆盖率仅作参考，不作为通过门槛。

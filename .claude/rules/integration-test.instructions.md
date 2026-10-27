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
| **剧本目录** | `tests/integration/scenarios/` — 四条道（lane）各跑一个独立账号，同时并行 |
| **覆盖率工具** | OpenCppCoverage（`--optimized_build`，Release PDB 行映射） |
| **报告产出** | `coverage_report_e2e_sample/` (HTML + Cobertura)、`coverage_history/e2e_sample.cov` |

---

## 2. 目录结构

```
tests/
└── integration/                    # 集成测试（E2E）
    ├── config.ini                  # 专用服务器配置
    ├── cli.conf                    # CLI 连接配置
    └── scenarios/                  # 剧本文件
        ├── register.sce            # 注册（每道各跑一次，产出 acct）
        ├── lane_foot.sce           # 道编排：include 串起本道片段
        ├── lane_skill.sce
        ├── lane_combat.sce
        ├── lane_social.sce
        ├── common/                 # 各道共用的起手：登录 / 观察问路 / 拜师
        ├── mountain_foot/          # 山脚基础：开箱 / 商人 / 任务 / 装备 / 茶棚 / 排行榜
        ├── provision/              # 各道在山脚自购补给（不共享库存）
        ├── skill/                  # 武学链：升级睡觉 / 基础与高级请教 / 激发与错误分支
        ├── combat/                 # 战斗尾：升级补气 / 打坐 / 树箱 / 偷袭 / 尸体与副本
        ├── social/                 # 社交生活：睡觉观察 / 毒药补药 / 钱庄 / 频道禁词
        └── house/                  # 房屋：申请入住 / 仆役 / 改名与卖房
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
4. **并行**注册 N 个账号（N 条道各跑一次 `register.sce`，各得各的 `acct`），随后
   **并行**开跑 N 条道（`lane_foot.sce` / `lane_skill.sce` / `lane_combat.sce` / `lane_social.sce`）
5. 向服务器主窗口发 `WM_CLOSE` 优雅关服
6. OpenCppCoverage 导出覆盖率报告
7. 控制台输出：测试结果、行覆盖率、未命中函数清单（超 60 个只报数字，完整清单写入文件）

### 3.2 常用参数

| 参数 | 说明 |
|------|------|
| `-SkipBuild` | 跳过 MSBuild，直接使用现有 `bin\jianghu.exe` 与 CLI |
| `-NoCoverage` | 跳过 OpenCppCoverage，快速调试剧本（不出覆盖率报告） |
| `-ResetDb` | 运行前重建测试库（需 `mysql.exe` 在 PATH；本机若不在，用 `-MysqlExe <路径>`） |
| `-Dbg` | 给各道 CLI 加 `--dbg`：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，用于查「客户端属性值和服务端对不上」 |
| `-CliExe <path>` | 指定 CLI 可执行文件路径 |
| `-StepTimeout <ms>` | 单步 wait/assert 超时（默认 20000） |
| `-ServerReadyTimeoutSec <sec>` | 服务器就绪等待上限（默认 180） |
| `-Workers <n>` | 并行账号（道）数，1~4，默认 4；调小会丢掉未启用道的覆盖 |

### 3.3 调试单个剧本

```powershell
# 快速跑一轮（不出覆盖率）；注意改过 cli\src 后不要加 -SkipBuild，否则用的是旧 CLI
.\run-integration-tests.ps1 -NoCoverage

# 手动跑单个片段：先看 lane_*.sce 里要哪些前置，再复用注册阶段生成的 vars
# （每道一份，在 %TEMP%\e2e_lanes\lane<N>.vars，含 acct/pass）
bin\cli\jianghu_cli.exe --config tests\integration\cli.conf `
  --script tests\integration\scenarios\skill\learn_force.sce `
  --vars $env:TEMP\e2e_lanes\lane2.vars --report report.txt
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
| `attr <名> <op> <值>` | 属性断言，支持 `hp` `max_hp` `eff_hp` `mp` `max_mp` `level` `exp` `combat_exp` `shen`（如 `attr eff_hp >= 489`） |
| `wield <物品> <槽位>` | 该物品真的装备在指定槽位（如 `wield 铁剑 右手`），槽位名与 `.info` 一致 |

### 4.2 变量机制

- **输入变量**：由 `run-integration-tests.ps1` 写入 `--vars` 文件（如 `name=xxx` `pass=yyy` `acct=123`）
- **产出变量**：`capture` 抓取的变量供后续剧本使用
- **引用语法**：`$name`、`$acct`、`$pass` 等

---

## 5. 四条道（lane）的职责与并行模型

一次运行**同时创建 N 个账号**（`-Workers`，默认 4）：先并行注册，再并行开跑各自的道。
每道 = 一个独立账号 + 一个 `lane_*.sce`（用 `include` 串起若干功能片段），**彼此不依赖
别的道留下的任何东西**——各道分别注册、分别自购补给、分别推进。

| 道 | 剧本 | 核心覆盖能力 |
|------|------|--------------|
| L1 | `lane_foot.sce` | 房间观察 / 向导问答与排行榜 / 开箱取物 / 商人买卖 / 任务 / 装备到右手 / 茶棚 / 住宅区置产到卖房 |
| L2 | `lane_skill.sce` | 拜师 / 高级技能 valid_learn 两道门槛 / study·enable·prepare·practice 分支 / 升级·补上限·睡觉 / force 与 sword 两线练到高级并激发 |
| L3 | `lane_combat.sce` | 升级补气 / 内功线与投掷线 / 打坐抬上限 / 树箱经验果 / 偷袭山鸡 / 尸体腐烂 / 副本进出 |
| L4 | `lane_social.sce` | 睡觉恢复 / 观察师父 / 毒药与补药数值对照 / 传送符 / 观察与说话作揖 / 钱庄数额 / 频道年龄与禁词 |

**⚠ 共享世界的隔离原则**：世界里稀缺且不可再生的资源必须**钉给唯一一道**——
山脚木箱与药箱（L1）、山顶树箱经验果与山鸡与副本（L3）、排行榜房间播报（L1）、
断肠散与传送符（L4）；商人库存按各道所需采购、合计不超上限。
**房间/频道播报型 `wait text`** 会串到同房间其他账号，只能靠两种方式免疫：
把断言写成**本人版**（`message_vision` 对本人渲染「你…」、对旁人渲染「<名字>…」，
故取「你…」前缀），或**把该播报钉给唯一一道**（如尸体腐烂、排行榜）。`notify_fail`、
`tell_object`、`say(msg, me)` 与 `attr`/`item`/`room`/`menu` 类条件天然只与本人相关。

**执行顺序**：`register.sce` 先并行注册出各道的 `acct`，随后各道并行跑自己的片段链。

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
- ❌ 断言只证明「命令发出去了」而不证明「结果对不对」（见下方 8.4）

### 8.4 并行下的断言纪律（新增/改动片段必过这三条）

1. **能用状态就别只看回执**：`attr` / `item` / `room` / `menu` / `object` 只与本人相关、最可靠。
   验「补上了没有」要断数值前后对比（`attr eff_hp >= 489`、`attr hp >= 363`），只断
   「你吃下一包金创药」证明不了血真的回来了；验装备要用 `wield 铁剑 右手`，只断 wield
   回执证明不了落在哪个槽位。
2. **文本断言要认得出「是我发的」**：`notify_fail`、`tell_object`、`say(msg, me)` 都私发、可直接断；
   `message_vision` 对本人渲染「你…」、对同房间旁人渲染「<名字>…」，所以取「你…」前缀才不会被
   别的账号串满足；改不动的房间/频道播报（尸体腐烂、排行榜）只能保证**只由本道触发**。
3. **稀缺资源唯一归属**：木箱与药箱（L1）、树箱经验果与山鸡与副本（L3）、库存很薄的商品
   （传送符×2、断肠散×5，L4）只能由一道采购与使用，并写进该道片段的头注释。

> `lua-sample/data/hooks.lua` 里的 `hooks.setup_human`（新角色起步本金）是**示例世界的正常
> 功能**：新角色靠它购物、置业、存款、自立门户。它不是测试脚手架，不要当「为测试加的逻辑」
> 删掉——删掉后多账号并行会因为全服只有一份山脚木箱金币而互相踩踏。

### 8.5 `/t` 目标是隐式状态（改片段时最容易踩）

- 目标由 `/t <名>` 设定、`/t 0` 清除，**一直保持到下一条 `/t` 为止**；片段之间不会重置，
  被 `include` 串起来后更是跨片段生效。
- 命令对目标的要求各不相同，必须对上号：
  | 命令 | 需要的目标 | 目标不对时的回执 |
  |---|---|---|
  | `learn` / `talk` | **房间里的角色**（`get_room_char`） | `你要向谁请教？` |
  | `study` | **自己背包里的物件**（`get_self_object`） | `你要读什么？` |
  | `use` / `give` / `wield` / `sell` | **物品** | （静默失败或回错分支文案） |
  | `practice` / `enable` / `prepare` / `upgrade` / `sleep` / `exercise` | 不使用目标 | — |
- ⚠ 往已有片段**插入会改目标的步骤**时（例如「学高级剑法前先 `/t 铁剑` + `/wield`」），
  必须在插入块的末尾把目标切回原对象，否则后续命令会拿到错目标。典型症状：
  `你要向谁请教？`、`你要读什么？`。
- ⚠ **高级技能 `valid_learn` 常带武器前置**：`sword_adv` 要手持剑、`throw_adv` 要手持暗器
  （`force_adv` 不要）。所以哪道要学哪条高级技能线，哪道就必须**自己买武器并在请教前装备**——
  木箱那把铁剑归 L1，别的道不能指望它。

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
| `你要向谁请教？` / `你要读什么？` | 改动片段时插入了会改目标的步骤、没把目标切回 | 1. 看失败行**之前最近的一条 `/t`** 2. 对照 8.5 的目标类型表核对 |
| `…必须手持剑类武器…学习高级…` | 高级技能 `valid_learn` 的武器前置没满足 | 1. 查该技能的 `valid_learn` 2. 确认本道 provision 有买、请教前有 `/wield` |
| `attr …` 超时 | 状态确实没到位，或改动后**没同步到客户端** | 1. 失败原因已附「（当前 名=值）」2. 拿当前值对比期望即可区分两种原因 |

---

## 10. 变更检查清单（提交前自查）

- [ ] 修改了服务端输出文本 → 同步更新 `tests/integration/scenarios/*.sce` 中的 `wait text`/`menu`
- [ ] 修改了交互流程/命令语法 → 同步更新剧本步骤
- [ ] 新增了 lua-sample 功能 → 在对应剧本添加覆盖步骤
- [ ] 往已有片段插入了会改 `/t` 目标的步骤 → 在块末尾把目标切回原对象（见 8.5）
- [ ] 某道要学高级技能线 → 本道 provision 自购对应武器，并在请教前 `/wield`（见 8.5）
- [ ] 运行 `.\run-integration-tests.ps1` 全绿
- [ ] 覆盖率 `delta ≥ 0`（对比 `coverage_history/summary-e2e-sample.txt`）
- [ ] 无新增「未命中函数」属于本次修改范围（或有合理解释）
- [ ] 没有在代码中保留注释形式的迁移记录

---

> **核心原则**：集成测试验证"真实服务器能否跑通真实业务流程"。覆盖率仅作参考，不作为通过门槛。

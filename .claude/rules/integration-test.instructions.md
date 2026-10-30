---
name: 'Integration Test'
description: 'Integration test guidelines for E2E scenario testing with OpenCppCoverage'
applyTo: '**/tests/integration/**'
---

# 集成测试指令文件

本文档是集成测试的**指令文件**：怎么跑、怎么写剧本、断言纪律、怎么排查。
架构、道模型、关键契约、覆盖率口径等设计内容见 `docs/design/integration-test.md`。

---

## 1. 架构与设计（指针）

架构总览、目录结构、组件职责见 **`docs/design/integration-test.md` §1**。
一句话版本：真实服务器 + 无头 CLI + 剧本驱动，入口 `run-integration-tests.ps1`，
专用配置在 `tests/integration/`，覆盖率由 OpenCppCoverage 在同一服务器进程上采集。

---

## 2. 道模型与设计（指针）

七条道的职责、并行模型、peer/spectator 注入机制、共享世界隔离原则见
**`docs/design/integration-test.md` §2**；`/t` 目标契约见其 §4；
服务器配置要点见其 §1.3；覆盖率口径与不测清单见其 §5/§6。
本文件 §8 的断言纪律与这些设计配套执行，改片段前先读对应设计条目。

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
3. 等待端口 6611 就绪（最长 180 秒，首次 PDB 加载较慢）
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
| `-ResetDb` | 运行前重建测试库：删 `e2e.db` 库文件，服务端启动自动重建（SQLite，无需外部工具） |
| `-Dbg` | 给各道 CLI 加 `--dbg`：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，用于查「客户端属性值和服务端对不上」 |
| `-CliExe <path>` | 指定 CLI 可执行文件路径 |
| `-StepTimeout <ms>` | 单步 wait/assert 超时（默认 20000） |
| `-ServerReadyTimeoutSec <sec>` | 服务器就绪等待上限（默认 180） |
| `-Workers <n>` | 并行账号（道）数，1~7，默认 7；调小会丢掉未启用道的覆盖，且 L5/L6 配对互动道必须成对启用、L5/L7 观战依附必须同时启用 |
| `-DurationRegressionSec <sec>` | 耗时回归阈值（默认 10）：本次剧本执行总耗时超前次同类运行超过该秒数时归因并给拆分建议 |

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

### 3.4 耗时监控（前次对比与拆分归因）

每次 **PASS** 运行都会把耗时写入 `coverage_history\e2e-timing-history.jsonl`（JSON Lines，
保留最近 50 条），记录剧本执行总耗时（= 注册 + 各道并行墙钟；构建/起服/覆盖率导出单列不参与
对比）、各道耗时/步数/场景清单（递归展开 `include`）。下一次运行与**前次同类通过运行**对比
（同 `-Workers` / `-NoCoverage` / `-Dbg` / `-StepTimeout` 才算同类，否则记为新基线）：

- **Δ ≤ 阈值**：仅打印对照，不归因。
- **Δ > 阈值（默认 10s）**：自动判断增长是否由**新增场景**导致——
  - 归因成立：点名新增的 `.sce`、识别关键路径道（最长道），并给**拆分探索**：
    方案 A（把新场景移入当前最短道）与方案 B（新增一条并行道），估算各自的墙钟与收益；
  - 关键路径未含新场景：说明增长来自步数变化 / 注册阶段 / 环境波动，并提示
    「拆分非关键道的新场景不会缩短墙钟」。
- 只有 **PASS** 运行入历史（失败运行耗时被中断截断，不可比）；报告同时写入
  `coverage_history\summary-e2e-sample.txt`。

拆分道之前必读 §8.4（稀缺资源唯一归属）与 §8.5（`/t` 目标复位）：拆出去的片段若依赖
钉给原道的资源（山脚木箱、树箱、排行榜等），会与并行账号互相踩踏。

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
| `attr <名> <op> <值>` | 属性断言，支持 `hp` `max_hp` `eff_hp` `mp` `max_mp` `level` `exp` `combat_exp` `shen`（如 `attr eff_hp >= 531`） |
| `wield <物品> <槽位>` | 该物品真的装备在指定槽位（如 `wield 铁剑 右手`），槽位名与 `.info` 一致 |

### 4.2 变量机制

- **输入变量**：由 `run-integration-tests.ps1` 写入 `--vars` 文件（如 `name=xxx` `pass=yyy` `acct=123`）
- **产出变量**：`capture` 抓取的变量供后续剧本使用
- **引用语法**：`$name`、`$acct`、`$pass` 等

---

## 5. 七条道（lane）的职责与并行模型（指针）

七条道（L1~L7）的职责表、并行模型、L5/L6 peer 机制、L7 spectator 机制、
共享世界的隔离原则、播报免疫方式见 **`docs/design/integration-test.md` §2**。
动手改片段时的配套纪律：稀缺资源唯一归属见 §8.4③，`/t` 目标复位见 §8.5，
互动断言七条见 §8.6。

---

## 6. 服务器配置要点（指针）

`tests/integration/config.ini` 的关键项（独立测试库、独立端口、`rate_limit=0`、
lua-sample 脚本根）与「命令行 `--config` 直接加载、不改写 `data/config.ini`」
的原则见 **`docs/design/integration-test.md` §1.3**。

---

## 7. 覆盖率参考（指针）

覆盖率的定位（参考手段、非通过门槛）、OpenCppCoverage 采集配置、参考指标、
未命中函数清单的口径与局限、缺口认知与明确不测清单见
**`docs/design/integration-test.md` §5/§6**。

---

## 8. 编写/维护剧本的规范

### 8.1 新增剧本步骤

1. **先在手工交互模式验证**流程可跑通
2. **记录每一步的服务端输出**（`wait text`/`wait menu` 依赖精确文本）
3. **同步一律用 `wait`**（20ms 轮询 + 超时），不要靠 `sleep` 赌时序
4. **设置合理 `timeout`**（默认 20s，复杂交互可加大）
5. **使用 `capture` 抓取动态值**（账号 id、物品 id 等）

#### sleep 的合法用途（`rate_limit=0` 之后仍必须用 sleep 的场景）

服务端命令限流虽已关闭（`config.ini` 的 `rate_limit=0`），但 `sleep` 并非因此失效——
它等的从来不是限流，而是**心跳驱动的游戏内计时**。服务端心跳 1 秒一拍
（`HeartBeat.cpp` 的 `alarm_loop`），Lua 的 `start_busy(n)` 以心跳为单位计时
（`start_busy(2)` ≈ 2 秒 busy），busy 期间几乎所有命令被 `is_busy()` 拒绝
（「你现在正忙着呢」）。所以：

- **唯一合法用途**：等 `start_busy` / 心跳脚本效果（吃药补血、打坐、经验果、
  尸体腐烂等跨拍生效的效果）走完。这类 sleep 的下限由 busy 时长/心跳拍数决定，
  症状是下一条命令被 `is_busy()` 拒——`wait` 等不到「忙完了」这种状态，只能等时间。
- **禁止**：`wait` 成功后再 `sleep`「缓冲」——`wait` 已完成同步，纯属白等；
  `cmd` 后直接 `sleep` 再 `menu`/`capture`——`menu`/`capture` 是 fail-fast 步骤，
  应改写为先 `wait menu`/`wait text` 兜住，而不是用 sleep 赌回包时机；
  `cmd`→`sleep`→`wait` 模式里的 sleep——`wait` 的轮询已覆盖，删掉即可。
- **`sleep` 不能代替 `wait`**：`wait` 是条件同步（等到为止），`sleep` 是盲等
  （睡够就走）。盲等要么早了（竞态）要么晚了（白费时长）。

### 8.2 修改现有剧本

- **修改服务端输出文本** → 必须同步更新剧本中的 `wait text`/`menu` 文本
- **修改交互流程**（增删菜单层级、改命令语法） → 必须同步更新剧本步骤
- **新增 lua-sample 功能** → 必须在对应剧本中添加覆盖步骤

### 8.3 禁止事项

- ❌ 在剧本中硬编码动态值（账号 id、物品 id、随机房间名）
- ❌ 省略 `wait` 直接 `cmd`（竞态导致不稳定）
- ❌ 使用 `sleep` 代替 `wait`；`wait` 成功后再加 `sleep`「缓冲」（见 8.1 的 sleep 纪律）
- ❌ 剧本之间有隐性依赖（除显式变量传递外）
- ❌ 在 `lua-sample` 目录下放测试脚手架（剧本、配置、脚本一律在 `tests/integration/`）
- ❌ 样本(lua-sample)是教材，不是测试夹具。禁止为了测试目的在sample中添加异常逻辑。
- ❌ **为了测试目的而强行修改生产代码（C++ 引擎 `src/`、公共 Lua 库 `lua-default/`）**。
  遇到测试不通过，先在**测试侧**想办法（调整剧本、补给、起步资金、`-ResetDb`、用 config
  配置项等）；确需动生产代码时，必须**从设计/架构角度说明其合理性**（是真 bug 或真设计
  缺陷，而非「让测试通过」的补丁——例如硬编码几个键名去保值、在通用函数里塞测试特判），
  并配套单元测试。为图省事在生产代码里打「测试友好」补丁是禁止的。
- ❌ 断言只证明「命令发出去了」而不证明「结果对不对」（见下方 8.4）

### 8.4 并行下的断言纪律（新增/改动片段必过这三条）

1. **能用状态就别只看回执**：`attr` / `item` / `room` / `menu` / `object` 只与本人相关、最可靠。
   验「补上了没有」要断数值前后对比（`attr eff_hp >= 531`、`attr hp >= 405`），只断
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

- 目标由 `/t <ID>` 设定、`/t 0` 清除，**一直保持到下一条 `/t` 为止**；片段之间不会重置，
  被 `include` 串起来后更是跨片段生效。**`/t` 只收 ID**：玩家=数字账号 ID（全服跨房间）、
  NPC/物品=英文 id（当前房间/背包）；显示名允许重名、不作目标（见 §8.6⑥）。
- 命令对目标的要求各不相同，必须对上号：
  | 命令 | 需要的目标 | 目标不对时的回执 |
  |---|---|---|
  | `learn` / `talk` | **房间里的角色**（`get_room_char`） | `你要向谁请教？` |
  | `study` | **自己背包里的物件**（`get_self_object`） | `你要读什么？` |
  | `use` / `give` / `wield` / `sell` | **物品** | （静默失败或回错分支文案） |
  | `practice` / `enable` / `prepare` / `upgrade` / `sleep` / `exercise` | 不使用目标 | — |
- ⚠ 往已有片段**插入会改目标的步骤**时（例如「学高级剑法前先 `/t sample_sword_basic`
  + `/wield`」），必须在插入块的末尾把目标切回原对象，否则后续命令会拿到错目标。
  典型症状：`你要向谁请教？`、`你要读什么？`。
- ⚠ **高级技能 `valid_learn` 常带武器前置**：`sword_adv` 要手持剑、`throw_adv` 要手持暗器
  （`force_adv` 不要）。所以哪道要学哪条高级技能线，哪道就必须**自己买武器并在请教前装备**——
  木箱那把铁剑归 L1，别的道不能指望它。

### 8.6 互动断言纪律（L5/L6/L7 互动道新增/改动片段必过这七条）

多客户端互动让「别人发的命令产生我这边的输出」成为常态，断言纪律在 §8.4 之上加严：

1. **私发通道优先**：`tell` 私聊、菜单、`notify_fail`、`tell_object`、`say(msg, me)`
   都只发本人，可直接 `wait text` 断言；只有拿不到私发回执时才退回房间版文本。
2. **房间版断言必须含对方角色名**：`message_vision` 对旁人渲染「<名字>…」，而角色名
   带 runhex 每轮唯一，所以断「<对方名>…」形式（经 `$peer` 拼出）不会被别的账号串满足；
   仍然要避免断「全房间广播里出现某个通用词」这类谁都能满足的条件。
3. **会合房间避开其他道断言**：互动道会合/停留的房间应选其他道**断言不依赖**的房间
   （当前选山脚茶棚——仅 L1 装水片段短暂经过且断言全为本人版）；若某片段要断房间
   播报，必须先确认该播报不会被并行道的路过账号干扰。
4. **超时信息写明「疑似对端未到位」**：配对道一侧 `wait`/`assert` 超时时，若失败点是
   会合（`wait object $peer`）或等待对方响应的步骤，排查先看**对端道**的日志（见 §9），
   不要只盯本道。
5. **稀缺资源归属**：互动道（L5/L6）不得采购/使用钉给唯一道的稀缺资源（木箱药箱、
   树箱经验果、山鸡、排行榜、断肠散、传送符）；互动双方要消耗的资源（如赠送的物品、
   摆摊的商品）由**互动道自己**的 provision 片段购买，声明写进道头注释。
6. **`/t <ID>` 只收 ID，不收显示名**（显示名允许重名、无法唯一定位）：
   - **玩家**：数字账号 ID（如 `/t 1396`、剧本用 `$peer_acct`），**全服有效、
     跨房间可用**——登录时服务端双向交换花名册（SENDUSER → 客户端自动
     downloaduser → DOWNUSER 回填账号进在线表），下线按 UID 广播 O=0 清理；
   - **NPC/物品等对象**：英文 id（对象 dbase"id"，即脚本 `set_name` 第二参；
     单参 set_name 回落 base_name），**当前环境有效**（所在房间 + 背包）；
   - `$peer`（角色名）仅用于**文本断言**（§8.6② 的含对方名渲染），
     一律不得作 `/t` 参数——剧本定向对方统一 `/t $peer_acct`
     （`run-integration-tests.ps1` 注册后互注到各道 vars）。
   同步手段：跨房间呼叫用 `/t $peer_acct` + `tell`；等待类步骤仍优先
   自触发（对方离房触发条件再回房）与宽 timeout 收敛的房间播报。
7. **`clear` 不得抹掉跨道信号**：`clear` 只用来清「本道自己命令的旧回执」，
   且不得落在「对方的广播/tell 可能已到达」与「消费它的 wait」之间——对端
   跑得比本道快时，跨道播报会先进本道缓冲，中途一条 `clear` 就把它抹掉，
   后面的 wait 干等超时且**播报永不重发**（实锤：pair 收尾的 clear 抹掉提前
   到达的离婚播报 → L5/L6/L7 三道连锁红）。信号消费方要么该段全程不 clear，
   要么保证 wait 先于信号可能到达的时刻启动；发送方不知道对端进度，
   别把次序寄托在「对方应该还没那么快」上。

---

## 9. 常见故障与排查

| 症状 | 常见原因 | 排查步骤 |
|------|----------|----------|
| `wait text` 超时 | 服务端输出文本变更、编码问题、限流丢弃指令 | 1. 查服务端日志 `log/` 2. 确认 `rate_limit=0` 3. 对比实际输出与剧本期望文本 |
| `wait menu` 超时 | 菜单未弹出、菜单项文本变更 | 1. 手工复现 2. 检查 Lua 脚本 `do_talk`/`menu` 分支 |
| `login` 失败 | 数据库未就绪、账号冲突、密码错误 | 1. `-ResetDb` 重建库文件 2. 确认 `vars.txt` 变量正确 |
| 服务器启动超时（180s） | PDB 加载慢、端口被占用、配置错误 | 1. `netstat -ano \| findstr 6611` 2. 检查 `config.ini` 路径 3. 手工起服看报错 |
| 覆盖率报告为空/异常 | 未加 `--optimized_build`、PDB 与 exe 不匹配、源码路径错 | 1. 确认同一次构建的 exe+pdb 2. `--sources src\*` 3. 重新构建 |
| CLI 连不上 | 端口错、host 错、服务器未就绪 | 1. 检查 `cli.conf` 2. 确认服务器日志有 `Listen on 6611` |
| `你要向谁请教？` / `你要读什么？` | 改动片段时插入了会改目标的步骤、没把目标切回 | 1. 看失败行**之前最近的一条 `/t`** 2. 对照 8.5 的目标类型表核对 |
| `…必须手持剑类武器…学习高级…` | 高级技能 `valid_learn` 的武器前置没满足 | 1. 查该技能的 `valid_learn` 2. 确认本道 provision 有买、请教前有 `/wield` |
| `attr …` 超时 | 状态确实没到位，或改动后**没同步到客户端** | 1. 失败原因已附「（当前 名=值）」2. 拿当前值对比期望即可区分两种原因 |
| 配对道 `wait object $peer` 超时 | **疑似对端未到位**：对端登录失败/中途失败停摆，或本道自己先失败没走到会合 | 1. 先看对端道的日志（`%TEMP%\e2e_lanes\lane<对端Id>.log`）2. 对端报告的 reason/失败行 3. 两边都正常再查会合房间名与 `$peer` 变量注入 |
| 观战道（L7）`wait text 比试开打/第二轮开打` 超时，或 L5 等 `比试观战完毕` 超时 | **疑似信号对端未到位**：L5 未发开战信号（比试/kill 段没跑到）或 L7 登录/移动停摆 | 1. 对端日志（L7 等信号看 `lane5.log`，L5 等回信号看 `lane7.log`）2. L7 是否已在山脚茶棚（`wait room` 行）3. `$spectator_acct`/`$peer_acct` 注入是否齐全（启动校验已拦一半未启用的情形） |
| 失败原因「服务器崩溃（未处理异常）」 | 服务器在关服路径（`WM_DESTROY`→等工作线程）崩溃；dump 由 `CCreateDump` 写出 | 1. 本轮新 dump **原地留在仓库根**（脚本会打印完整路径，不会挪走）；上一轮的旧 dump 已归档到仓库根 `crash_dumps\` 2. 用 `bin\jianghu.pdb` + WinDbg/cdb 分析 3. 崩溃发生在剧本判定之后，剧本本身可能是绿的，别只看剧本结果 |

---

## 10. 变更检查清单（提交前自查）

- [ ] 修改了服务端输出文本 → 同步更新 `tests/integration/scenarios/*.sce` 中的 `wait text`/`menu`
- [ ] 修改了交互流程/命令语法 → 同步更新剧本步骤
- [ ] 新增了 lua-sample 功能 → 在对应剧本添加覆盖步骤
- [ ] 往已有片段插入了会改 `/t` 目标的步骤 → 在块末尾把目标切回原对象（见 8.5）
- [ ] 改动 L5/L6 配对道或 L7 观战道片段 → 过互动断言纪律七条（私发优先 / 含对方名 / 会合房间 / 超时排查对端 / 资源归属 / `/t` 只用 ID / `clear` 不抹跨道信号，见 8.6）；L5↔L7 互为开战信号两端（`WitnessId`/`PeerId`），一方超时先查对端日志（§9）
- [ ] 某道要学高级技能线 → 本道 provision 自购对应武器，并在请教前 `/wield`（见 8.5）
- [ ] 运行 `.\run-integration-tests.ps1` 全绿（含「服务器崩溃」检测——剧本绿但服务器崩了同样算失败）
- [ ] 新增场景后总耗时较前次同类运行增长 > 10s → 看「耗时监控」的归因与拆分建议，评估是否拆道（见 3.4）
- [ ] 覆盖率 `delta ≥ 0`（对比 `coverage_history/summary-e2e-sample.txt`）
- [ ] 无新增「未命中函数」属于本次修改范围（或有合理解释）
- [ ] 没有在代码中保留注释形式的迁移记录

---

> **核心原则**：集成测试验证"真实服务器能否跑通真实业务流程"。覆盖率仅作参考，不作为通过门槛。

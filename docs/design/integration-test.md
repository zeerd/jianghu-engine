# 集成测试架构设计（E2E）

> 本文档是集成测试的**设计文档**：架构、道模型、关键契约、覆盖率口径——即"为什么这么设计"。
> 动手操作请看指令文件 `.claude/rules/integration-test.instructions.md`（运行方式、剧本 DSL、
> 断言纪律、排查、提交前清单）。

---

## 1. 架构总览

集成测试采用 **真实服务器 + 无头 CLI 客户端 + 剧本驱动** 架构：

```
┌─────────────────────────────────────────────────────────────────┐
│                    run-integration-tests.ps1                    │
│  (构建 → OpenCppCoverage 起服 → 跑剧本 → 优雅关服 → 导出报告)   │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
       ┌────────────┐  ┌────────────┐  ┌────────────────┐
       │  服务器    │  │  CLI 客户端 │  │ SQLite 文件库   │
       │ jianghu.exe│  │jianghu_cli │  │ e2e.db（自动    │
       │ (Release)  │  │  (Release) │  │ 建库建表）      │
       └────────────┘  └────────────┘  └────────────────┘
              │               │
              ▼               ▼
       ┌────────────────────────────────┐
       │      tests/integration/        │
       │  config.ini  (专用服务器配置)   │
       │  cli.conf    (CLI 连接配置)     │
       │  scenarios/  (剧本文件 *.sce)   │
       └────────────────────────────────┘
```

### 1.1 关键组件

| 组件 | 说明 |
|------|------|
| **入口脚本** | `run-integration-tests.ps1`（仓库根目录） |
| **服务器配置** | `tests/integration/config.ini` — 独立测试库、独立端口、lua-sample 脚本根 |
| **CLI 配置** | `tests/integration/cli.conf` — 连接 `127.0.0.1:6611` |
| **剧本目录** | `tests/integration/scenarios/` — 七条道（lane）各跑一个独立账号，同时并行（L5/L6 为配对互动道、L7 为观战道） |
| **覆盖率工具** | OpenCppCoverage（`--optimized_build`，Release PDB 行映射） |
| **报告产出** | `coverage_report_e2e_sample/` (HTML + Cobertura)、`coverage_history/e2e_sample.cov` |

### 1.2 目录结构

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
        ├── lane_interaction_a.sce  # 配对互动道（L5/L6，peer 互注）
        ├── lane_interaction_b.sce
        ├── lane_spectate.sce       # 观战道（L7，单向依附 L5：peer/spectator 注入）
        ├── common/                 # 各道共用的起手：登录 / 观察问路 / 拜师
        ├── interaction/            # 互动道片段：会合 / 组队 / 私聊 / 赠送 / 交易
        ├── mountain_foot/          # 山脚基础：开箱 / 商人 / 任务 / 装备 / 茶棚 / 排行榜
        ├── provision/              # 各道在山脚自购补给（不共享库存）
        ├── skill/                  # 武学链：升级睡觉 / 基础与高级请教 / 激发与错误分支
        ├── combat/                 # 战斗尾：升级补气 / 打坐 / 树箱 / 偷袭 / 尸体与副本
        ├── social/                 # 社交生活：睡觉观察 / 毒药补药 / 钱庄 / 频道禁词
        └── house/                  # 房屋：申请入住 / 仆役 / 改名与卖房
```

### 1.3 服务器配置要点（`config.ini`）

```ini
[database]
type = sqlite                       ; SQLite 文件库，零外部服务依赖（CI/CD 友好）
path = tests/integration/e2e.db     ; 服务端启动自动建库建表

[server]
port = 6611               ; 独立端口，避免与开发服务冲突
start_room = 青云山山脚    ; lua-sample 起始房间
rate_limit = 0            ; 关闭指令限流，避免边界随机丢弃导致 wait 超时

[lua]
script_root = lua-sample          ; 被测样本
default_script_root = lua-default ; 兜底默认脚本
```

**关键原则**：服务端通过命令行 `--config tests\integration\config.ini` 直接加载，**不再临时改写
`data/config.ini`**——测试配置是独立资产，不污染业务配置。

**数据库为 SQLite 文件库**：`type = sqlite` 时服务端走 `SqliteDb.cpp`（sqlite3 静态编入 exe），
启动自动建库文件与全部表、播种账号 ID 起点；`-ResetDb` 即删 `e2e.db` 文件。E2E 不再需要
任何外部数据库服务，托管 CI 机器开箱即跑（MySQL 方言路径退出 E2E 覆盖，见 §6）。

---

## 2. 道模型（lane）与并行机制

一次运行**同时创建 N 个账号**（`-Workers`，默认 7）：先并行注册，再并行开跑各自的道。
每道 = 一个独立账号 + 一个 `lane_*.sce`（用 `include` 串起若干功能片段）。L1~L4 **彼此
不依赖别的道留下的任何东西**——各道分别注册、分别自购补给、分别推进；L5/L6 是唯一的
例外：两道**互相依赖对方的角色名**；L7 观战道单向依赖 L5。

**执行顺序**：`register.sce` 先并行注册出各道的 `acct`，随后各道并行跑自己的片段链。
墙钟时间约为最长的一道而非各道之和。

### 2.1 各道职责

| 道 | 剧本 | 核心覆盖能力 |
|------|------|--------------|
| L1 | `lane_foot.sce` | 房间观察 / 向导问答与排行榜 / 开箱取物 / 商人买卖 / 任务 / 装备到右手 / 茶棚 / 住宅区置产到卖房 |
| L2 | `lane_skill.sce` | 拜师 / 高级技能 valid_learn 两道门槛 / study·enable·prepare·practice 分支 / 升级·补上限·睡觉 / force 与 sword 两线练到高级并激发 |
| L3 | `lane_combat.sce` | 升级补气 / 内功线与投掷线 / 打坐抬上限 / 树箱经验果 / 偷袭山鸡 / 尸体腐烂 / 副本进出 |
| L4 | `lane_social.sce` | 睡觉恢复 / 观察师父 / 毒药与补药数值对照 / 传送符 / 观察与说话作揖 / 钱庄数额 / 频道年龄与禁词 |
| L5 | `lane_interaction_a.sce` | 配对互动 A：PK 前置（`pk_prep`：起步历练 + 青锋剑 + 冲 10 级，赢家账号专属）→ 与 L6 会合后覆盖组队 / 私聊屏蔽 / 定向说话表情 / 赠送 / 做客邀请 / 婚姻 / 摆摊交易与 PK 死亡链 |
| L6 | `lane_interaction_b.sce` | 配对互动 B：响应方——接受组队 / 回私聊 / 购买 A 的商品 / 应战切磋与 PK 死亡链（输家账号保持 1 级徒手） |
| L7 | `lane_spectate.sce` | 观战（PK 第三方，零采购零学艺）——比试房观战 `guard` / 观战退出 `halt` / 战斗房加入 `join`（+`ready_pk`、留场至散场）/ 随 `end_fight` 散出；只在会合房间待命收 L5 的开战信号 |

### 2.2 配对互动道（L5/L6）的 peer 机制

`run-integration-tests.ps1` 在注册阶段把两道的角色名互相写入对方 vars 文件的 `peer` 键
（如 L5 的 vars 含 `peer=e2e<runhex>6`），注册完成后回读 acct 再互注
`peer_acct=<对方数字账号 ID>`。两个键分工：**`$peer_acct` 供 `/t` 定向**（/t 只收 ID，
见 §4），**`$peer` 供文本断言**（含对方名的渲染）；**禁止硬编码对方角色名或账号**
（runhex 每轮都变）。

会合片段 `interaction/rendezvous.sce` 用「`wait room` + `wait object $peer`（宽 timeout）」
松同步——服务端 `on_enter` 双向推 ADDOBJECT，后到者进门瞬间双方同时满足条件。

L5/L6 必须成对启用（`-Workers` ≥ 6，或同时选中 5、6），只启用一半脚本会直接报错。

### 2.3 观战道（L7）的 spectator 机制

L7 的 vars 注入 `peer`/`peer_acct` = L5（`PeerId=5`），L5 的 vars 反向注入
`spectator`/`spectator_acct` = L7（`WitnessId=7`）；两个键都由注册后回读 acct 注入。
分工：**`$spectator_acct` 供 L5 跨房下发开战信号**（`/t $spectator_acct` + `/tell`，
比试/两次 PK 的时序全靠它串），`$peer*` 供 L7 回信号与文本断言。L5/L6 与 L7 必须
同时启用（`-Workers` ≥ 7，或同时选中 5、6、7），缺一半脚本启动即报错。

### 2.4 共享世界的隔离原则

世界里稀缺且不可再生的资源必须**钉给唯一一道**——山脚木箱与药箱（L1）、山顶树箱经验果
与山鸡与副本（L3）、排行榜房间播报（L1）、断肠散与传送符（L4）；商人库存按各道所需
采购、合计不超上限。

**互动道（L5/L6/L7）不得触碰以上任何钉给唯一道的资源**，会合房间内不做资源操作，
自购补给走互动道自己的 provision 片段。

**房间/频道播报型 `wait text`** 会串到同房间其他账号，只能靠两种方式免疫：把断言写成
**本人版**（`message_vision` 对本人渲染「你…」、对旁人渲染「<名字>…」，故取「你…」前缀），
或**把该播报钉给唯一一道**（如尸体腐烂、排行榜）。`notify_fail`、`tell_object`、
`say(msg, me)` 与 `attr`/`item`/`room`/`menu` 类条件天然只与本人相关。
（断言纪律的操作条款见指令文件 §8。）

> `lua-sample/data/hooks.lua` 里的 `hooks.setup_human`（新角色起步本金）是**示例世界的
> 正常功能**：新角色靠它购物、置业、存款、自立门户。它不是测试脚手架——删掉后多账号
> 并行会因为全服只有一份山脚木箱金币而互相踩踏。

---

## 3. combat_rule 数据集契约（与 E2E 相关部分）

`attack_gate`（出手准入）是 CombatRules 契约下**单一询问点**的实例：C++ 只申报时机、
规则归属数据集。原本 `CCmdStandard::kill` 问 `hooks.combat_rule("attack_gate")`、而玩家
互斗的等级门槛却硬编码在 `CChar::cmd_hit_ob`，两处准入并存；统一后 `cmd_hit_ob` 问一次
`attack_gate`，等级限制由数据集声明——**lua-default** 数据集实现 75 级新手保护（默认
世界行为不变），**lua-sample** 数据集不设门槛（世界观设定；出手后果照常走 PK 罪恶、
守卫、死亡结算）。由此 `kill` 玩家 PK、死亡链、`join_battle` 成功分支在 E2E 变为可达。

---

## 4. `/t` 目标契约

`/t` 只收 ID，不收显示名（显示名允许重名、无法唯一定位）：

- **玩家**：数字账号 ID（如 `/t 1396`、剧本用 `$peer_acct`），**全服有效、跨房间可用**；
- **NPC/物品等对象**：英文 id（对象 dbase"id"，即脚本 `set_name` 第二参；单参 `set_name`
  回落 base_name），**当前环境有效**（所在房间 + 背包）。

服务端配套：登录 `on_send_online` 双向互推 DOWNUSER（服务端直推、免 SENDUSER→
downloaduser 往返突发撞限流）；下线 `notify_offline` 广播 SENDUSER O=0 按 UID 摘除
（关服路径 `g_terminated` 拦下）。CLI 配套：`find_target` 只按 ID 匹配；USERLIST
（/who 分页）为合并式，不清空丢账号字段。

游戏内提示只写规则限定（`/t <id> …`），不作设计解释。剧本侧的目标类型表与复位纪律
见指令文件 §8.5/§8.6⑥。

---

## 5. 覆盖率口径

> **覆盖率只是一个方便的用来确认功能是否被覆盖的手段。** 这是因为恰巧 C++ 支持这么个
> 技术，所以我们拿来用了。它不是铁律。毕竟我们没办法拿到 Lua 覆盖率。真正用来确认
> 功能覆盖度的永远是设计和代码自身，不是覆盖率数字。

### 5.1 采集配置

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

统计时按叶子文件名跳过 `LuaSyntaxChecker.cpp` 与 `CmdWizard*.cpp`（巫师指令），
口径对齐 `run-coverage.ps1` 的 `$excludedFiles`。

### 5.2 参考指标（非硬性门槛）

| 指标 | 说明 |
|------|------|
| **行覆盖率** | 参考上一次提交（`coverage_history/summary-e2e-sample.txt` 的 `delta`），大幅下降需排查原因 |
| **新增代码** | 应有对应剧本步骤覆盖，以确认功能可跑通 |
| **修改代码** | 应确保修改分支被剧本触达，以确认改动生效 |
| **报告附带** | 提交时附上覆盖率报告截图或关键数据，供参考 |

### 5.3 未命中函数清单的口径与局限

`run-integration-tests.ps1` 每轮把「未命中函数」清单写入
`coverage_history\e2e-sample-unhit-functions.txt`。读清单前先知道它的近似性：

- **统计源是 OpenCppCoverage 的二进制导出**，函数粒度按 PDB 符号解析；Release `/OPT:ICF`
  会折叠等价函数、`/OPT:REF` 会剔除未引用函数，清单里的"未命中"可能指向折叠后的合并符号。
- **Lua 绑定（`l_*`）占未命中的大头**：绑定函数只有对应脚本调用该 API 时才可达，
  教材（`lua-sample`/`lua-default`）没调用的绑定天然 0 命中——这是**内容需求**问题，
  不是测试缺口（见 §6 不测清单）。
- **一道 PASS ≠ 覆盖到位**：剧本断言的是行为契约，未命中清单看的是函数触达；两者互补，
  谁也不是对方的充分条件。
- 清单只回答「哪些函数这次没跑到」，不回答「该不该跑到」——该不该由 §6 的设计决策回答。

### 5.4 覆盖率缺口认知（来自 PoC 验证）

> **E2E 覆盖率的缺口是系统性的，不是操作姿势问题。** E2E 覆盖率不设数字门槛
> （见 §5.2 的定位：参考手段，非通过门槛）；单测口径的覆盖率门槛（及格/良好线）
> 与 delta ≥ 0 的增量门禁以 `.claude/rules/unit-test.instructions.md` 为准。
>
> E2E **无法覆盖**的典型功能（需单测兜底）：
> - 巫师命令（`CmdWizard*.cpp`）
> - 副本管理、住宅系统（部分）
> - MySQL 方言路径（`MySqlDb.cpp`、`OLD_PASSWORD`/`LAST_INSERT_ID` 等 mysql 分支）——
>   E2E 用 SQLite，mysql 后端与方言分支单测兜底
> - 心跳/连接管理（单测已覆盖）
>
> 历史上曾列为「E2E 无法覆盖」的组队/夫妻/互教/互给（多客户端）、物品命令边界分支，
> 已随 L5/L6 配对互动道与 L1/L4 片段补齐——多客户端能力正是道模型扩展的产出。

---

## 6. 明确不测清单（设计决策）

以下功能**有意**不进 E2E，保持 0 命中属可接受缺口：

| 项 | 理由 |
|---|---|
| **巫师指令全部**（CmdWizardSystem / CmdWizardAdmin / CmdWizardWorld / `snoop` / SnoopSystem） | 决策：不测巫师指令 |
| LuaSyntaxChecker | 只在 `--check-lua` 命令行模式运行，单测已覆盖（`test_lua_checker.cpp`） |
| MySqlDb / MySQL 方言分支 | E2E 用 SQLite（CI/CD 零外部服务依赖），mysql 后端与方言分支单测覆盖 |
| CreateDump / Logger / HeartBeat alarm / db 断连 | 基础设施生命周期，单测兜底 |
| `l_*` Lua 绑定中教材未调用的部分 | 教材是教材不是测试夹具，不为测试加调用；教材该有而没有的功能另行提内容需求 |
| `CReward::victim_penalty` | 需要「NPC 击杀玩家」或无凶手死亡；样本世界可战斗房间的 NPC 全部钉给唯一道，互动道没有可输的对手。**不为测试挪动资源归属**，单测兜底（`test_reward` KillerReward_NpcSlaysUser_PenaltyApplied） |
| `AutoFight` hatred/vendetta/aggressive/berserk | hatred 需「打过 NPC 后再次撞面」、aggressive 的进取型 NPC 只在副本（L3 独占进出）、vendetta 教材无人设置 `vendetta_mark`、berserk 无触发源——E2E 没有合法触发面，单测已覆盖分支（`test_auto_fight`） |
| 组队/夫妻状态拖入 PK 战斗房（`auto_follow_fight` 的两条拖入分支） | 会令入队方落入两队籍（`join_team` 无跨队去重），破坏赢家账号的确定性；单测已钉（`test_fightroom_team`），E2E 的 PK 段在组队解散后的干净状态下进行 |

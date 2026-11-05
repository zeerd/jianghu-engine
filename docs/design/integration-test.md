# 集成测试架构设计（E2E）

> 本文档是集成测试的**设计文档**：架构、道模型、关键契约、覆盖率口径——即"为什么这么设计"，
> 外加参数表（§1.7）与剧本 DSL 词汇表（§7）这两份随源头维护的清单。
> 动手操作请看指令文件 `.claude/rules/integration-test.instructions.md`（运行与调试方式、
> 剧本编写与断言纪律、排查、提交前清单）。

---

## 1. 架构总览

集成测试采用 **真实服务器 + 无头 CLI 客户端 + 剧本驱动** 架构：

```
┌─────────────────────────────────────────────────────────────────┐
│                    run-integration-tests.ps1                    │
│  (构建 → OpenCppCoverage 起服 → 注册/各道并行 ┐ persist 段→重启  │
│   → 优雅关服 → 多路覆盖率合并通行证导出)      └ reload 改文件→断言 │
│                                 （两个试验台均与各道并行）        │
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
| **剧本目录** | `tests/integration/scenarios/` — 十条道（lane）各跑一个独立账号，同时并行（L5/L6 为配对互动道、L7 为观战道、L8 为依附 L3 的山贼试炼道、L9 为巫师服务道（账号自举不注册）、L10 为练功道（N4 自 L3 拆出）） |
| **覆盖率工具** | OpenCppCoverage（`--optimized_build`，Release PDB 行映射） |
| **报告产出** | `coverage_report_e2e_sample/` (HTML + Cobertura)、`coverage_history/e2e_sample.cov` |

### 1.2 目录结构

```
tests/
└── integration/                    # 集成测试（E2E）
    ├── config.ini                  # 专用服务器配置（主服 6611）
    ├── cli.conf                    # CLI 连接配置
    ├── config.persist.ini          # persist 试验台配置（6612，独立库）
    ├── cli.persist.conf
    ├── config.reload.ini           # reload 试验台配置（6613，独立库 + 私有脚本副本）
    ├── cli.reload.conf
    ├── reload_testbed.ps1          # reload 试验台编排（可前台直跑调试剧本）
    ├── reload_fixtures/            # 热重载 fixture 变体（只读模板，a/b/c/broken）
    ├── log-allowlist.txt           # 服务端 ERROR 白名单（登记制）
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
        ├── house/                  # 房屋：申请入住 / 仆役 / 改名与卖房 / 直卖收口
        ├── raid/                   # 山贼试炼：战前准备 / 闯寨开战
        ├── persist/                # 挑战计数跨重启验证（pre/post 两段，重启前后各跑一次）
        ├── reload/                 # 热重载剧本链（改文件→等 sys 广播→断言行为，单链跑完）
        └── wizard/                 # 巫师道：道首自验 / 服务链与链尾房间重整
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

### 1.4 失败判定通道（四条，缺一不可）

剧本 PASS 只说明「客户端收到了期望的回包」——它天然看不见服务端内部的报错与
进程状态。整轮运行的失败判定由四条独立通道汇聚（任一非 PASS 即整轮 FAIL，
预算通道除外——它是护栏不是门禁）：

| 通道 | 捕获什么 | 机制 |
|------|---------|------|
| **剧本报告/退出码** | 业务流程没跑通（wait 超时、断言失败） | 各道 CLI `--report status=` + 退出码兜底 |
| **崩溃 dump** | 剧本判定之后的进程死亡（关服路径 UAF 等） | 启动前快照仓库根 `*.dmp`，停服后 diff，新增即 FAIL；dump 原地保留作证据 |
| **服务端日志** | 服务端侧报错（Lua 异常、命令失败、DB 错误）——剧本收不到这些 | 启动前记 `tests/integration/log\` 字节偏移，停服后只扫新增区间：未登记 ERROR/FATAL 即 FAIL（白名单登记制，见指令 §3.5）；WARN 仅计数报告。每轮以 `--log-run-id` 启动，日志单独落 `M-D-<runid>.txt` 且报告点名该文件 |
| **绝对预算** | 耗时慢性爬升（滑动基线放行机制失效） | `-DurationBudgetSec`（默认 420s）超限高声告警；调预算走 diff 评审（见 §2.5） |

设计取舍：服务端日志与崩溃 dump 是「剧本绿但系统坏」的唯一两双外部眼睛——
崩溃检测先例（关服 UAF：剧本全绿而进程已死，6/6 轮仍判 FAIL）证明这类矛盾
必须由剧本之外的通道裁决。日志通道对**预期错误**（负测等）用白名单登记而非
整体放行：登记动作迫使每个预期报错被显式点名与归属，未知即红。登记也不是
遮瑕：疑似 bug 先查根因、不许直接拉黑——`set_leader` 的自我设定本是设计内
「清除跟随」惯用法，曾被误打 LOG_ERROR 污染错误日志，查实后修级别并以单测
钉住契约。

### 1.5 persist 试验台（并发中间重启机制）

**目的**：验证 `lua-sample/npc/sample_bandit_boss.lua` 的 `can_save` 存盘——黑风寨主
的挑战计数每次有玩家进房触发其 `init` 就 +1 并 `Save` 落 `t_npc`，重启服务器后
`create()` 末尾的 `self:Load()` 必须恢复计数、继续累加。每轮运行都清库重来，
「跨重启」只能由测试自己制造一次重启。

**架构：独立服务器 + 后台任务 + N 段循环**（文首第 10 条有执行细节）：
- **独立实例**：试验台跑在自己的端口（`config.persist.ini`，6612）与自己的
  SQLite 库（`e2e_persist.db`）上——与主套件（6611/e2e.db）零共享，主套件
  对试验台的重启完全无感，反之亦然。「重启不影响其他 lane」由**进程隔离**
  结构性保证，而非时序安排。
- **后台任务**：`Start-Job` 驱动的自足任务（起服/注册/跑段/停服/重启/导出/
  结果 JSON 全在任务内），在阶段 1 之前启动，**与注册和全部道并行**——重启
  发生在各道跑到一半时（真正的「中间重启」），不占关键路径（段与重启耗时只
  作诊断展示，不进 `exec_sec`）。
- **N 段循环**：段表 `$PersistSegments` 每项 = 试验台的一个服务器生命周期，
  段间即一次重启；本次两段（`persist_pre` → 重启 → `persist_post`）。后续需要
  三段（pre→重启→mid→重启→post）或其他重启/干净态测试，加一段表项即可，
  循环、注入（`Inject`）、覆盖率合并自动适配。

**断言链**：段 1 触发计数并 `capture n` → 段前注入 `expect_n=n+1`（任务读上一段
vars 计算，DSL 无算术）→ 段 2 断言喊话是「第 `${expect_n}` 个」——重启后
`Load` 恢复了计数才可能成立。

**剧本结构契约**（persist 剧本头注有完整依据）：大厅 boss 由房间 init 在**首次
进厅**时刷出，而 `on_enter` 先遍历既有对象、后跑房间 init——首进时 boss 尚不
存在、其 init 不触发；故两段都走**同实例回环**（首进刷出 → 步行出厅 → 二进
触发）。出厅依赖大厅 `valid_leave` 的拦截恒放行（`query` 对字符串 dbase 值恒
返回 0）——该拦截若被修好，persist 会以「还有敌人在场」超时显式暴露。

**观测口径**：dump 基线与日志字节偏移各记一次，主服与试验台全部实例（含
`-pt` 后缀的试验台日志）的增量一并纳入崩溃/ERROR 扫描。覆盖率多路采集——
主服一路、persist 试验台每段一路、reload 试验台一路、各实例只导二进制——
全部结束后跑一趟**合并通行证**（`--input_coverage` 可多次传入、但 OCC 启动时
校验文件存在，故必须等所有实例导出完毕）产出最终 html/cobertura/binary。

### 1.6 reload 试验台（热重载 E2E）

**目的**：热重载的主路径是「改脚本文件 → 心跳自动重载 → 行为切换」，大多数
场景不需要主动敲 `reload` 命令；同链覆盖失败兜底（语法错/运行错/缺文件保留
旧版本）、手动回执如实上报、共享模块 require 缓存语义（自动重载换不掉、
`reload all` 才换）与 mtime-noop 不误触发。

**架构：独立服务器 + 私有脚本副本 + 一条连续剧本链**（文首第 11 条有执行细节）：
- **独立实例**：`config.reload.ini`（6613）+ 独立库（`e2e_reload.db`）+ 私有
  脚本副本（`test_tmp\e2e_reload\scripts` = lua-sample 每轮整树复制 + fixture
  A 变体）。`reload all`、坏文件、共享模块修改都发生在副本上——教材
  `lua-sample` 与生产 `lua-default` 全程只读，主套件对试验台零感知
  （进程隔离，与 persist 同理）。
- **改文件在剧本里**：DSL 新增 `filecopy`/`filedelete`（目标白名单钉死在
  `test_tmp/`，笔误写不进教材/生产目录）——改文件 → 等重载 → 断言行为的
  因果同处一个片段；编排层只做剧本做不到的事（整树重建副本、起停服、跑链、
  日志断言）。整树重建而非增量覆盖：上一轮残留的坏文件会让本轮开服即报错。
- **等待用 sys 广播屏障**：自动重载结果广播到 sys 频道（wiz_only，仅巫师
  可见；巫师账号先 `cmd /sys` 发言完成订阅——发言即订阅），剧本 `wait text
  自动重载完成/失败…` 等屏障而非 sleep 盲等；失败广播沿 `hasError` 状态
  转换只报一次（坏文件每 5s 重试到修好为止，逐次广播会把频道刷成噪音）。
- **与各道并行**：后台任务（`reload_testbed.ps1`，亦可前台直跑快速迭代剧本），
  链约 120~180s，远低于最长道，不占关键路径；结果 JSON 并入失败判定四通道。
- **日志断言归任务**：auto-reload 行在案（机制在跑）、noop 探针从未被误
  重载、失败锚点 ERROR 在案——只有日志能回答的三类；剧本只断言客户端可见
  行为。

**结果语义契约**（生产侧 `reload_script` 系，三出口共用同一成败语义）：失败
保留旧 ref 但必须回传「未成功」——旧 ref ≥0 恒为真，拿返回值当真值会把失败
谎报成成功（自动重载日志/实例通知、巫师回执、sys 广播都曾被带偏）；失败不
通知实例（否则坏文件每 5s 让实例拿旧版本重建一次）；`reload all` 回执带失败
计数。**require 缓存的清除必须同时换掉 `registry["_LOADED"]` 与
`package.loaded` 两张引用**——require 的读写走前者（`luaopen_package` 初始
两者同表），只重绑 `package.loaded` 字段等于没清，共享模块永远换不新（E2E
`shared_base` 首跑即钉出的存量缺陷）。**三条路径（手动单发 / `reload all` /
自动重载）的副作用必须同构**：状态缓存（skill/condition）摘除走同一出口、
房间名索引重建都在成功路径上；mtime 快筛比较用 FILETIME（100ns）粒度——
秒级比较会把「重载同秒的最后一次保存」判成没变而漏掉。**日志分层**：错误
锚点行（脚本名 + 原因）保持 ERROR 供白名单锚定，回溯帧（`dump_caller_info`）
是上下文、降为 INFO——否则一次失败产出十几条泛化 ERROR，白名单只能靠会
遮蔽真异常的泛化签名过闸。

### 1.7 入口脚本参数（`run-integration-tests.ps1`）

> **参数的唯一登记处是脚本 `param` 块**；本表随脚本同步维护，是给读者看的说明书。
> 新增/修改参数：改脚本 `param` + 本表，**指令文件不列参数**（分工见 copilot-instructions 铁律）。

| 参数 | 说明 |
|------|------|
| `-SkipBuild` | 跳过 MSBuild，直接使用现有 `bin\jianghu.exe` 与 CLI |
| `-NoCoverage` | 跳过 OpenCppCoverage，快速调试剧本（不出覆盖率报告） |
| `-ResetDb` | 运行前重建测试库：删 `e2e.db` 库文件，服务端启动自动重建（SQLite，无需外部工具） |
| `-Dbg` | 给各道 CLI 加 `--dbg`：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，用于查「客户端属性值和服务端对不上」 |
| `-CliExe <path>` | 指定 CLI 可执行文件路径 |
| `-Simple` | 冒烟模式：单条道验证起服→登录→look→行走→关服；仅供人类快速确认，AI/Agent 禁用于验收 |
| `-StepTimeout <ms>` | 单步 wait/assert 超时（默认 20000） |
| `-ServerReadyTimeoutSec <sec>` | 服务器就绪等待上限（默认 180） |
| `-Workers <n>` | 并行账号（道）数，默认 10；按前 N 条启用，依赖缺半启动即报错（见 §2） |
| `-DurationBudgetSec <sec>` | 剧本执行总耗时的绝对预算（默认 420s），超限告警不判 FAIL（见 §2.5） |
| `-DurationRegressionSec <sec>` | 前次对比的 Δ 归因阈值（默认 10s），纯诊断不参与判定（见 §2.5） |

---

## 2. 道模型（lane）与并行机制

一次运行**同时创建 N 个账号**（`-Workers`，默认 10）：先并行注册，再并行开跑各自的道。
按 `$Lanes` 前 N 条启用；**依赖缺半启动即报错**（PeerId/WitnessId/WizardId 三类校验，
L2 起声明 `WizardId=9`）——故实际可用值为 1（仅 L1）或 ≥9。
每道 = 一个独立账号 + 一个 `lane_*.sce`（用 `include` 串起若干功能片段）。L1~L4 **彼此
不依赖别的道留下的任何东西**——各道分别注册、分别自购补给、分别推进；L5/L6 是唯一的
互依对（两道**互相依赖对方的角色名**）；L7 观战道单向依赖 L5、L8 山贼试炼单向依赖
L3（黑风寨错峰的握手信号，见 §2.4）。L9 巫师服务道（账号自举、不注册）接受
L2/L3/L4/L5/L8/L10 的**单向服务请求**（`/t 1` + `tell` → 固定顺序 wait → 私发回执，
见 §2.6）；L10 练功道自包含（N4 自 L3 拆出的五段练功链，见 §2.1）。

**执行顺序**：`register.sce` 先并行注册出各道的 `acct`，随后各道并行跑自己的片段链。
墙钟时间约为最长的一道而非各道之和。

### 2.1 各道职责

| 道 | 剧本 | 核心覆盖能力 |
|------|------|--------------|
| L1 | `lane_foot.sce` | 房间观察 / 向导问答与排行榜 / 开箱取物 / 商人买卖 / 任务 / 装备到右手 / 茶棚 / 住宅区置产到卖房 |
| L2 | `lane_skill.sce` | 拜师 / 高级技能 valid_learn 两道门槛 / study·enable·prepare·practice 分支 / 升级·补上限·睡觉 / force 与 sword 两线练到高级并激发 |
| L3 | `lane_combat.sce` | 树箱经验果 / 偷袭青蛇（throwing 技能与 mp 由 L9 供能、`/enable` 自做）/ 战利品与口粮（青蛇掉蛇胆→服下验蛇毒状态、包子验食物回血）/ 尸体腐烂 / 副本进出（与 L8 双向频道握手）——N4 拆分后练功五段迁 L10，本道只剩战斗/世界职责 |
| L4 | `lane_social.sce` | 睡觉恢复 / 观察师父 / **NPC 师徒 `family/user` 回归**（巫师直读 dbase 断言 0，玩家收徒场景的反向锚点）/ 毒药与补药数值对照（含涂毒挂 `poison/modu`：巫师供毒术+内力、自持铁剑）/ 传送符 / 观察与说话作揖 / 钱庄数额 / 频道年龄与禁词 / answer 频道 hook 接管（`exam.lua` 天下大考：出题→答错→答对+`attr combat_exp` 断奖励入账） |
| L5 | `lane_interaction_a.sce` | 配对互动 A：**玩家收徒（A 扮师父——资格前置 `master_recruit_prep` 拜师青云子+巫师供能 `recruit_exp`，随后 recruit→rexp→expell→重拜四段，全在会合房间完成）** → PK 前置（`pk_prep`：起步历练 + 青锋剑 + 冲 10 级，赢家账号专属）→ 与 L6 会合后覆盖组队 / 私聊屏蔽 / 定向说话表情 / 赠送 / 做客邀请 / 婚姻 / 摆摊交易与 PK 死亡链 |
| L6 | `lane_interaction_b.sce` | 配对互动 B：响应方——**玩家收徒（B 扮徒弟：兑现收徒并存盘、收开革回执、先发起重拜）** / 接受组队 / 回私聊 / 购买 A 的商品 / 应战切磋与 PK 死亡链（输家账号保持 1 级徒手） |
| L7 | `lane_spectate.sce` | 观战（PK 第三方，零采购零学艺）——比试房观战 `guard` / 观战退出 `halt` / 战斗房加入 `join`（+`ready_pk`、留场至散场）/ 随 `end_fight` 散出；只在会合房间待命收 L5 的开战信号 |
| L8 | `lane_raid.sce` | 山贼试炼（N2 新增）：注册 con30 天赋流（Attrs 参数化）→ 历练冲级（`/upgrade` 自做，max_hp 1260 是战斗窗口地基）→ 战前顶 hp → force 技能/内力由 L9 供能（拆两次请求，enable 清 mp 的顺序硬约束）+ `/enable`/`/enforce` 自做 → **闯黑风寨与双山贼开战**（败退/重伤均收尾）→ 双向频道握手让位 L3。**降断言设计**：剧本只验「开战 → 战斗收束 → 战后信号」，不反验出招/反震——`l_perform_action` 已烧、`l_receive_wound` 结构性不可达（§6 内力经济行），副本第二入口窗口与内力经济实测归本道 |
| L9 | `lane_wizard.sce` | 巫师服务（N4 新增，账号自举不注册）：巫师一号（cc_id=1）随服启动自举，按**固定 wait 顺序**收各需求道的 `/t 1` + `tell` 服务请求 → 执行 → 私发回执——服务面：收徒资格（`call set recruit_exp`，L5）/ NPC 师徒家门查询（`call query family/user`，回执带值，L4）/ 冲级（`call set level`，L5）/ 毒术+内力（`set_skill poison`+`call set mp`，L4）/ 山贼技能+内力（`set_skill`+`call set`，L8 两段）/ 偷袭（`set_skill throwing`+`call set mp`，L3）/ 组合（`set_skill` 四技能，L2）/ 练功（`call set max_mp`，L10）。请求词与 capture 标记全服唯一，服务段全程不 clear（§2.6） |
| L10 | `lane_train.sce` | 练功道（N4 自 L3 拆出）：upgrade 门槛链（`levelup_heal`）/ force 链（`force_for_exercise`）/ 两轮收工打坐（`exercise_max_mp`）/ halt 保 mp（`halt_exercise`）/ 收工不抬升补全（`exercise_no_raise`，L9 供大上限）/ throwing 链（`learn_throwing`，棋子自购自 wield）——原 L3 的练功/学习任务唯一覆盖源 |

### 2.2 配对互动道（L5/L6）的 peer 机制

`run-integration-tests.ps1` 在注册阶段把两道的角色名互相写入对方 vars 文件的 `peer` 键
（如 L5 的 vars 含 `peer=e2e<runhex>6`），注册完成后回读 acct 再互注
`peer_acct=<对方数字账号 ID>`。两个键分工：**`$peer_acct` 供 `/t` 定向**（/t 只收 ID，
见 §4），**`$peer` 供文本断言**（含对方名的渲染）；**禁止硬编码对方角色名或账号**
（runhex 每轮都变）。

会合片段 `interaction/rendezvous.sce` 用「`wait room` + `wait object $peer`（宽 timeout）」
松同步——服务端 `on_enter` 双向推 ADDOBJECT，后到者进门瞬间双方同时满足条件。

L5/L6 必须成对启用，只启用一半启动即报错（PeerId 校验）。

### 2.3 观战道（L7）的 spectator 机制

L7 的 vars 注入 `peer`/`peer_acct` = L5（`PeerId=5`），L5 的 vars 反向注入
`spectator`/`spectator_acct` = L7（`WitnessId=7`）；两个键都由注册后回读 acct 注入。
分工：**`$spectator_acct` 供 L5 跨房下发开战信号**（`/t $spectator_acct` + `/tell`，
比试/两次 PK 的时序全靠它串），`$peer*` 供 L7 回信号与文本断言。L5/L6 与 L7 必须
同时启用，缺一半启动即报错（WitnessId/PeerId 校验）。

### 2.4 共享世界的隔离原则

世界里稀缺且不可再生的资源必须**钉给唯一一道**——山脚木箱与药箱（L1）、山顶树箱经验果
与青蛇及其掉落的蛇胆（L3）、排行榜房间播报（L1）、蛇胆与传送符（L4）；商人库存按各道所需
采购、合计不超上限。**黑风寨副本是唯一双道共享的稀缺资源**：L3 只在链尾路过大厅
找小头目离开（不打山贼），山贼战与庭院首刷归 L8——两道靠**双向频道握手**错峰
（L3 到寨口广播「L3已到寨口」→ L8 战后收到才回广播「黑风寨已清」→ L3 收到才
进寨），使两道在单例副本上**不同时间开战/通行**；握手的设计依据是 clear 抹信号的
教训（L8 战后信号早于 L3 的消费 wait 3 分钟会被 L3 沿途 clear 吞掉；纪律条款见
指令文件 §8.6⑦），双向先广播再回话让两个到达都落在对端 wait 已启动的窗口内。

**互动道（L5/L6/L7）不得触碰以上任何钉给唯一道的资源**，会合房间内不做资源操作，
自购补给走互动道自己的 provision 片段。

**房间/频道播报型 `wait text`** 会串到同房间其他账号，只能靠两种方式免疫：把断言写成
**本人版**（`message_vision` 对本人渲染「你…」、对旁人渲染「<名字>…」，故取「你…」前缀），
或**把该播报钉给唯一一道**（如尸体腐烂、排行榜）。`notify_fail`、`tell_object`、
`say(msg, me)` 与 `attr`/`item`/`room`/`menu` 类条件天然只与本人相关。
（断言纪律的操作条款见指令文件 §8。）

> `hooks.setup_human`（新角色起步本金）是示例世界的正常功能、不是测试脚手架——删掉后
> 并行账号会踩踏全服唯一一份金币（警告全文见指令文件 §8.4）。

### 2.5 耗时模型与绝对预算

**模型**：剧本执行总耗时 = 注册墙钟（各账号并行跑同一份 `register.sce`）+ 各道并行
墙钟（取**最长道**，即关键路径）。构建/起服/覆盖率导出单列，不计入。

**关键路径与物理地板（N4 L3 拆分后，2026-11-01）**：拆分把原 L3 的五段练功（force/
throwing 技能链、打坐链、upgrade 门槛）迁往 L10 后，「390s 且拆不掉」的地板已不存在——
当前实测**关键路径是 L2 学艺链与 L3 山顶战斗尾双子、轮间互换（225~232s）**（请教/服药
循环与「树箱随机 + 击杀后 120 拍腐烂锚定 + 副本握手」各是同一账号的串行前置，都拆不掉）、
L8 221s、L10 179s。新增场景默认不得进 L2/L3（两条都在 230s 上下且互相只差秒级——报告
吸收能力行当前约 7s）；L1/L4/L5/L6/L7 有数百秒吸收余量，先往那里放。

**预算语义（对滑动基线的修正）**：曾用「较前次同类运行 Δ≤10s」当门禁，其结构性
缺陷是**每次放行 10 秒都合法、总时长单调爬升无上限**——合法值是滑动的，护栏跟着
被告走。修正为**绝对预算 `-DurationBudgetSec`（默认 420s）**：

- 预算是**代码里的常数**——调整必须改脚本默认值走 diff 评审，抬预算的动作留下
  显式记录，无法隐性发生；
- 超预算**高声告警不判 FAIL**：预算是护栏（防慢性爬升）不是门禁（瞬时环境波动
  不该红），收敛压力来自告警的显眼与报告的归因；
- 前次 Δ 对比降级为**纯诊断**：超参考阈值时给新增场景归因与拆分探索（方案 A 移入
  最短道 / 方案 B 新开道），但不参与通过判定。

报告固定三行硬指标（预算/实测/余量、关键路径道、吸收能力），把「加片段往哪放」
的余量显式化——新增场景只要不落关键路径，吸收能力内加满也不动墙钟。字面格式：

```
预算 420s / 实测 NNNs / 余量 +NNs
关键路径: L<x> <道名> NNNs ｜ 次长道 NNNs ｜ 非关键路径可承接约 NNNs 新增而不改墙钟
```

**观测产出（脚本实现契约）**：每次 **PASS** 运行把耗时追加进
`coverage_history\e2e-timing-history.jsonl`（保留最近 50 条；仅 PASS 入史——失败运行
被中断截断不可比），记录总耗时、预算值与各道耗时/步数/场景清单（递归展开 `include`）；
同 `-Workers`/`-NoCoverage`/`-Dbg`/`-StepTimeout` 且 PASS 才算同类基线。报告同时写
`coverage_history\summary-e2e-sample.txt`。Δ 超阈值时脚本自动归因：命中新增场景则点名
`.sce` 并给方案 A/B 估算；未命中则归因到步数变化/注册阶段/环境波动，并提示拆分非关键道
的新场景不会缩短墙钟。

**L8 握手的双峰结构已收敛（N4 L3 拆分后，2026-11-01）**：历史上 L8 链尾 wait L3 的
寨口广播使 L3（345s）与 L8（344s）收尾同步、非关键路径吸收能力降到秒级；拆分后
L3=229s / L8=221s 双双退出关键路径（L8 战后等 L3 寨口广播的握手不变，见 §2.4），
总时长 233s、预算余量 +187s。但**吸收能力行当前只有约 2s**——不是双峰造成的，而是
次长道 L3（229s）紧咬关键路径 L2（231s）；加场景前先看当轮报告的吸收能力行，能力
不足时走拆分探索（方案 B 新开道）或抬预算（改脚本默认值走 diff 评审）。

### 2.6 巫师服务协议（L9 ↔ 需求道）

N4 起，有特别战斗需求的道不再自行刷技能/等级/内力，改为向 L9 巫师账号请求：

- **请求**：需求道在服务点 `/t 1`（巫师数字 ID 恒 1）+ `tell <请求词>，<唯一标记>账号是 $acct`，
  发完即进 `wait text <回执词>`（宽 timeout 300~360s，覆盖失败路径）。
- **服务**：L9 按**固定 wait 顺序**逐段收请求（收徒资格 L5 → NPC 师徒家门查询 L4 →
  冲级 L5 → 毒术/内力 L4 → 山贼技能/内力 L8 → 偷袭 L3 → 剑气/组合/激励 L2 →
  练功 L10，按需求道到达时刻排列），`capture <唯一标记> as <var>` 抓账号后
  `/t $acct` 执行巫师命令（`set_skill`/`call set`/`full`），再私发回执；
  **读回**类服务（`call query <key>`）先 `wait` 回包再 capture 数字、把值拼进回执，
  需求道据此断值——dbase 没有用户侧只读命令，这是唯一读回通道。
- **乱序兜底**：DSL 无分支——先到的请求滞留缓冲（L9 服务段全程不 clear，§2.4 的教训），
  「固定顺序 + 请求词/capture 标记全服唯一」保证多请求先后不保证时不抓错账号。
- **回执消费**：需求道 `wait text` 回执词后用 `wait attr` 断言真实状态（§5 状态优先）。
- **干扰面**：`set_skill`/`call set` 走 sys 频道广播——实测只推巫师本人（各道 channels
  不含 sys、服务端不推给非订阅者），需求道无假匹配。
- **顺序硬约束**（需求道侧声明）：L8 的 `/enable force` 会清 mp → 技能/max_mp 供能在
  enable **前**、mp 供能在 enable **后**，L8 拆两次请求。

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
  回落 base_name），**当前环境有效**（所在房间 + 背包）；**房间视角与背包存在同名对象时
  先认背包**（同名异物会出现在房间视角，自己的物件以背包为准）。

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

统计时按叶子文件名跳过**整文件结构性不可达**的六个文件——`LuaSyntaxChecker.cpp`、
`CmdWizard*.cpp`、`SnoopSystem.cpp`（巫师域）、`MySqlDb.cpp`（E2E 用 SQLite）、
`CreateDump.cpp`（正常轮不触发）、`AutoFight.cpp`（§6 无合法触发面）——行覆盖率分母与
未命中清单**双通道同过**该过滤；函数级不测（Channel snoop、托盘 UI、alarm、run_id、
nullify_vote、victim_penalty、mv 等）**保留分母**，只在清单里以 §6 决策认领表达。
单测口径 `run-coverage.ps1` 的 `$excludedFiles`（多 main.cpp/Sql.cpp、无巫师与
SnoopSystem/AutoFight）与 E2E 差异是**有意的**：被测对象不同——单测对巫师域 90%+ 有覆盖。

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
- **缺口甄别必须排除「定义/注释/同名撞车」**：对「教材是否调用了某绑定」做文本检索时，
  字段定义、注释行、模块同名函数都会命中 `key(` 形态，判据应为「非 function 定义、
  非注释行的 `[.:]key(`」且必须全量扫（取前 N 处会漏掉后面的真调用）。
- **概率触发分支单轮不保证命中**：NPC 战斗行为里的 `random(n)` 分支（如山贼
  `perform_action` 的自疗/出招）在单轮 E2E 里掷骰次数不足即 0 命中——属随机性而非可达性
  缺口；补测手段（战斗延时拖关键路径）与代价要按 §2.5 耗时预算权衡后再定。

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

以下**域**有意不进 E2E，保持 0 命中属可接受缺口。本表按**域/机制**给裁定，不逐函数列——
逐函数的认领明细属于「补集成测试」阶段的查缺补漏工具（`coverage_history/e2e-unhit-claimed.txt`
与每轮未命中清单），阶段收口后无长期意义，不进本表：

| 域 | 裁定与理由 |
|---|---|
| **巫师命令域**（CmdWizard* / `snoop` / SnoopSystem） | 不追求覆盖度：L9 验的是服务协议本身，巫师命令只是协议的工具面，服务链用到的 `full`/`set_skill`/`call` 已覆盖（副产品）；余项保持清单可见防盲区，不设消化目标 |
| **教材未调用的 `l_*` Lua 绑定**（含 Lua→C++ 显式调用胶水、零继承者基类、未注册 hook、教材未定义字段） | 教材是教材不是测试夹具，不为测试加调用；教材该有而没有的功能走内容需求通道（作者点名 → 教材补内容 → 剧本自然覆盖）。0 命中是内容需求问题，非测试缺口 |
| **单机 E2E 结构性不可达**（`vote` 通过分支与 `nullify_vote` 超时、房产按天计费与离线 owner 重建） | 投票每 IP 只计 1 票使「≥3 票」需多机多 IP；计费按天、房间 reset 900 秒一轮超出单轮时长——环境与时长尺度限制，非可达性问题；vote 入口/门槛/去重分支已由剧本覆盖 |
| **依赖特定世界状态/资源归属的触发面**（`victim_penalty`、`AutoFight` 主动敌意类、组队/夫妻状态拖入 PK 战斗房） | 样本世界可战斗 NPC 全部钉给唯一道、进取型 NPC 只在 L3 独占副本、教材不设 `vendetta_mark`/berserk 触发源——E2E 没有合法触发面；**不为测试挪动资源归属与世界设定**，单测兜底；E2E 的 PK 段保持组队解散后的干净状态，不踩双队籍 |
| **内力经济/公式封顶挡住的战斗分支**（`l_receive_wound` 反震、`do_roar` 狮子吼、`do_sneak` 同队检查） | 反震在 `mp_you ≤ 3mp` 封顶下 damage ≥ 1 恒成立（数学不可达，非概率问题）；`do_roar` 的 mp≥200 超出 `mp ≤ 2×max_mp` 的经济封顶；同队检查被前置 PvE 收窄与 `query_team` 0 歧义挡住——均为引擎机制的结构性结论，**不为覆盖而改游戏机制**，机制设计修订才重开；C++ 调用点随正常分支已覆盖 |
| **低价值协议透传与不存在的入口**（`mv`/`loc` 地图定位、`info`/`item` 命令） | `loc` 核心副作用每次登录已覆盖（ADDOBJECT 链）；`mv` 无分支无状态机且无头 CLI 不发坐标，补发只测「透传不崩」；`info`/`item` 经查无声明无实现无注册——无代码可测，记录不测 |
| **死代码与零调用方** | 全库无调用方/无生产者的函数（含触发即 ERROR 的兜底路径）没有入口，E2E 无从可达——**禁止为覆盖触发**；逐条登记在认领清单（阶段工具），不进本表 |
| **基础设施与外部依赖**（CreateDump / Logger / HeartBeat alarm / db 断连 / LuaSyntaxChecker / MySQL 方言） | 生命周期与方言路径单测兜底；E2E 用 SQLite（CI/CD 零外部服务依赖），`--check-lua` 是独立命令行模式 |

---

## 7. 剧本 DSL 契约

> **动词与 wait 条件的唯一实现方是 CLI**（`cli/src/scenario_steps.cc` 等）；本节是给
> 剧本作者看的契约说明书，随 CLI 实现同步维护。**新增动词/条件：改 CLI + 本节，
> 指令文件不维护 DSL 词汇表**（分工见 copilot-instructions 铁律）。怎么写好剧本
> （wait 纪律、sleep 用法、断言纪律）见指令文件 §8。

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
| `sleep` | `sleep <ms>` | 节流 / 信令间隙 / 无信号时间等待（合法边界见指令 §8.1） |
| `filecopy` | `filecopy <源> <目标>` | 复制覆盖文件（热重载片段切换脚本变体用）。**目标只允许落在 `test_tmp/` 或 `tmp/` 暂存区**，写进教材/生产目录一律拒绝；源取首个空白前的 token、目标取行尾余下全文（目标可含空格）；路径经 `$var` 替换 |
| `filedelete` | `filedelete <路径>` | 删除文件（缺文件兜底场景）。同样的暂存区白名单；目标不存在即失败（路径笔误必须立刻暴露） |
| `include` | `include <file>` | 复用其他剧本文件 |
| `#` | `# 注释` | 行注释 |

### 7.1 wait 条件类型

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
| `busy` / `not_busy` | 等待忙碌起止：`busy`=等到进入忙碌，`not_busy`=等到忙碌结束（拍数走完/中断放行）。两者都由服务端 STATE 的翻转推送驱动。**分段到达**：动作回执（命令中途发送）与 STATE（命令末 `CUser::execute_command` 尾 `send_off`，`User.cpp:426`）分两段到达 CLI——`wait text` 命中回执时起始 STATE 可能仍在途，紧接的 `wait not_busy` 会拿旧快照误判「不忙」立即通过（由此拒绝回执的下一条命令被 `is_busy()` 拒）。判别三种姿势：① **单分支必 busy 点**（回执只有成功文案、`start_busy` 必发生）→ 回执后 `wait busy` 收下起始（轮询天然容忍在途）再 `wait not_busy`，零盲等；② **双分支点**（成功/拒绝回执，拒绝支不产生 busy、`wait busy` 必卡超时）→ 回执后 `sleep 500` 跳过在途间隙（间隙上界=命令剩余执行+网络，毫秒级，500ms 裕量充足）再 `wait not_busy`；③ `wait text` 等到的是**更晚的服务端消息**（下一条命令的回执、`battle_end` 等——同 TCP 有序流 ⟹ 起始 STATE 必已到达）或前有秒级等待 → 直接 `wait not_busy` |
| `attr <名> <op> <值>` | 属性断言，支持 `hp` `max_hp` `eff_hp` `mp` `max_mp` `level` `exp` `combat_exp` `shen`（如 `attr eff_hp >= 531`） |
| `wield <物品> <槽位>` | 该物品真的装备在指定槽位（如 `wield 铁剑 右手`），槽位名与 `.info` 一致 |

### 7.2 变量机制

- **输入变量**：由 `run-integration-tests.ps1` 写入 `--vars` 文件（如 `name=xxx` `pass=yyy` `acct=123`）
- **产出变量**：`capture` 抓取的变量供后续剧本使用
- **引用语法**：`$name`、`$acct`、`$pass` 等
| **热重载不覆盖的脚本域**（`lua-default` 实例脚本不参与自动重载与 `reload all`；`data/hooks.lua`、`constants.lua`、根脚本 `damage_msg.lua` 任何 reload 都不覆盖） | 前者是生产脚本保护（`fromDefault` 跳过，改动靠手动 `reload <名>` 或重启；其 require 类共享模块仍随 `reload all` 清 require 缓存换新）；后者开机直载不进 `m_scripts`——**改这些文件需重启是设计边界**，E2E 不为触发而重启服务器 |

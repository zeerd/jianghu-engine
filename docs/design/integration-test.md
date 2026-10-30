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
| **剧本目录** | `tests/integration/scenarios/` — 十条道（lane）各跑一个独立账号，同时并行（L5/L6 为配对互动道、L7 为观战道、L8 为依附 L3 的山贼试炼道、L9 为巫师服务道（账号自举不注册）、L10 为练功道（N4 自 L3 拆出）） |
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

### 1.4 失败判定通道（四条，缺一不可）

剧本 PASS 只说明「客户端收到了期望的回包」——它天然看不见服务端内部的报错与
进程状态。整轮运行的失败判定由四条独立通道汇聚（任一非 PASS 即整轮 FAIL，
预算通道除外——它是护栏不是门禁）：

| 通道 | 捕获什么 | 机制 |
|------|---------|------|
| **剧本报告/退出码** | 业务流程没跑通（wait 超时、断言失败） | 各道 CLI `--report status=` + 退出码兜底 |
| **崩溃 dump** | 剧本判定之后的进程死亡（关服路径 UAF 等） | 启动前快照仓库根 `*.dmp`，停服后 diff，新增即 FAIL；dump 原地保留作证据 |
| **服务端日志** | 服务端侧报错（Lua 异常、命令失败、DB 错误）——剧本收不到这些 | 启动前记 `tests/integration/log\` 字节偏移，停服后只扫新增区间：未登记 ERROR/FATAL 即 FAIL（白名单登记制，见指令 §3.5）；WARN 仅计数报告 |
| **绝对预算** | 耗时慢性爬升（滑动基线放行机制失效） | `-DurationBudgetSec`（默认 420s）超限高声告警；调预算走 diff 评审（见 §2.5） |

设计取舍：服务端日志与崩溃 dump 是「剧本绿但系统坏」的唯一两双外部眼睛——
崩溃检测先例（关服 UAF：剧本全绿而进程已死，6/6 轮仍判 FAIL）证明这类矛盾
必须由剧本之外的通道裁决。日志通道对**预期错误**（负测等）用白名单登记而非
整体放行：登记动作迫使每个预期报错被显式点名与归属，未知即红。

---

## 2. 道模型（lane）与并行机制

一次运行**同时创建 N 个账号**（`-Workers`，默认 10）：先并行注册，再并行开跑各自的道。
每道 = 一个独立账号 + 一个 `lane_*.sce`（用 `include` 串起若干功能片段）。L1~L4 **彼此
不依赖别的道留下的任何东西**——各道分别注册、分别自购补给、分别推进；L5/L6 是唯一的
互依对（两道**互相依赖对方的角色名**）；L7 观战道单向依赖 L5、L8 山贼试炼单向依赖
L3（黑风寨错峰的握手信号，见 §2.4）。L9 巫师服务道（账号自举、不注册）接受
L2/L3/L5/L8/L10 的**单向服务请求**（`/t 1` + `tell` → 固定顺序 wait → 私发回执，
见 §2.6）；L10 练功道自包含（N4 自 L3 拆出的五段练功链，见 §2.1）。

**执行顺序**：`register.sce` 先并行注册出各道的 `acct`，随后各道并行跑自己的片段链。
墙钟时间约为最长的一道而非各道之和。

### 2.1 各道职责

| 道 | 剧本 | 核心覆盖能力 |
|------|------|--------------|
| L1 | `lane_foot.sce` | 房间观察 / 向导问答与排行榜 / 开箱取物 / 商人买卖 / 任务 / 装备到右手 / 茶棚 / 住宅区置产到卖房 |
| L2 | `lane_skill.sce` | 拜师 / 高级技能 valid_learn 两道门槛 / study·enable·prepare·practice 分支 / 升级·补上限·睡觉 / force 与 sword 两线练到高级并激发 |
| L3 | `lane_combat.sce` | 树箱经验果 / 偷袭山鸡（throwing 技能与 mp 由 L9 供能、`/enable` 自做）/ 尸体腐烂 / 副本进出（与 L8 双向频道握手）——N4 拆分后练功五段迁 L10，本道只剩战斗/世界职责 |
| L4 | `lane_social.sce` | 睡觉恢复 / 观察师父 / 毒药与补药数值对照 / 传送符 / 观察与说话作揖 / 钱庄数额 / 频道年龄与禁词 |
| L5 | `lane_interaction_a.sce` | 配对互动 A：PK 前置（`pk_prep`：起步历练 + 青锋剑 + 冲 10 级，赢家账号专属）→ 与 L6 会合后覆盖组队 / 私聊屏蔽 / 定向说话表情 / 赠送 / 做客邀请 / 婚姻 / 摆摊交易与 PK 死亡链 |
| L6 | `lane_interaction_b.sce` | 配对互动 B：响应方——接受组队 / 回私聊 / 购买 A 的商品 / 应战切磋与 PK 死亡链（输家账号保持 1 级徒手） |
| L7 | `lane_spectate.sce` | 观战（PK 第三方，零采购零学艺）——比试房观战 `guard` / 观战退出 `halt` / 战斗房加入 `join`（+`ready_pk`、留场至散场）/ 随 `end_fight` 散出；只在会合房间待命收 L5 的开战信号 |
| L8 | `lane_raid.sce` | 山贼试炼（N2 新增）：注册 con30 天赋流（Attrs 参数化）→ 历练冲级（`/upgrade` 自做，max_hp 1260 是战斗窗口地基）→ 战前顶 hp → force 技能/内力由 L9 供能（拆两次请求，enable 清 mp 的顺序硬约束）+ `/enable`/`/enforce` 自做 → **闯黑风寨与双山贼开战**（败退/重伤均收尾）→ 双向频道握手让位 L3。**降断言设计**：剧本只验「开战 → 战斗收束 → 战后信号」，不反验出招/反震——`l_perform_action` 已烧、`l_receive_wound` 结构性不可达（§6 拍板 B 行），副本第二入口窗口与内力经济实测归本道 |
| L9 | `lane_wizard.sce` | 巫师服务（N4 新增，账号自举不注册）：巫师一号（cc_id=1）随服启动自举，按**固定 wait 顺序**收各需求道的 `/t 1` + `tell` 服务请求 → 执行 → 私发回执——服务面：冲级（`call set level`，L5）/ 山贼技能+内力（`set_skill`+`call set`，L8 两段）/ 偷袭（`set_skill throwing`+`call set mp`，L3）/ 组合（`set_skill` 四技能，L2）/ 练功（`call set max_mp`，L10）。请求词与 capture 标记全服唯一，服务段全程不 clear（§2.6） |
| L10 | `lane_train.sce` | 练功道（N4 自 L3 拆出）：upgrade 门槛链（`levelup_heal`）/ force 链（`force_for_exercise`）/ 两轮收工打坐（`exercise_max_mp`）/ halt 保 mp（`halt_exercise`）/ 收工不抬升补全（`exercise_no_raise`，L9 供大上限）/ throwing 链（`learn_throwing`，棋子自购自 wield）——原 L3 的练功/学习任务唯一覆盖源 |

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
与山鸡（L3）、排行榜房间播报（L1）、断肠散与传送符（L4）；商人库存按各道所需
采购、合计不超上限。**黑风寨副本是唯一双道共享的稀缺资源**：L3 只在链尾路过大厅
找小头目离开（不打山贼），山贼战与庭院首刷归 L8——两道靠**双向频道握手**错峰
（L3 到寨口广播「L3已到寨口」→ L8 战后收到才回广播「黑风寨已清」→ L3 收到才
进寨），使两道在单例副本上**不同时间开战/通行**；握手的设计依据是 §8.6⑦ 的
clear 抹信号教训（L8 战后信号早于 L3 的消费 wait 3 分钟会被 L3 沿途 clear 吞掉），
双向先广播再回话让两个到达都落在对端 wait 已启动的窗口内。

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
的余量显式化——新增场景只要不落关键路径，吸收能力内加满也不动墙钟。

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
- **服务**：L9 按**固定 wait 顺序**逐段收请求（冲级 L5 → 山贼技能/内力 L8 → 偷袭 L3 →
  组合 L2 → 练功 L10，按需求道到达时刻排列），`capture <唯一标记> as <var>` 抓账号后
  `/t $acct` 执行巫师命令（`set_skill`/`call set`/`full`），再私发回执。
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
- **缺口甄别必须排除「定义/注释/同名撞车」**（N1 实证的判据陷阱）：对「教材是否调用了
  某绑定」做文本检索时，`function M.key(...)` 的**字段定义**、`-- hooks.key(...)` 的
  **注释**、以及 `book.create(...)` 这类**模块同名函数**都会命中 `key(` 形态——它们都不是
  对注入绑定的调用。判据应为「非 function 定义、非注释行的 `[.:]key(`」，且**必须全量扫**
  （取前 N 处会漏掉后面的真调用）。N1 按此复扫后 17 个 D 候选切为 9 个定义撞车（并入
  「教材未调用」）+ 8 个真未触发。
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

以下功能**有意**不进 E2E，保持 0 命中属可接受缺口：

| 项 | 理由 |
|---|---|
| **巫师命令域**（CmdWizardSystem / CmdWizardAdmin / CmdWizardWorld / `snoop` / SnoopSystem） | **N4 收口决策（2026-11-01）：不追求巫师命令覆盖度。** L9 引入的对象是**服务协议**本身（需求道 `/t 1`+`tell` 请求 → 固定顺序 wait → 私发回执的请求/回执链——已由 10 道 E2E 全绿验证），巫师命令只是协议的工具面；服务链自然用到的 `full`/`set_skill`/`call` 已覆盖，属**副产品**。余 38 条（Admin 14 / System 18 / World 6）**保持清单可见**（CmdWizard* 留在排除表外、逐轮列示防盲区）**但不设消化目标、不计欠账**；`snoop`/SnoopSystem 同不测（监听窥屏无剧本语义面）。沿革：整域不测（旧决策）→ N4 立项移出排除表实测 → 收口定为「可见、不追求」 |
| LuaSyntaxChecker | 只在 `--check-lua` 命令行模式运行，单测已覆盖（`test_lua_checker.cpp`） |
| MySqlDb / MySQL 方言分支 | E2E 用 SQLite（CI/CD 零外部服务依赖），mysql 后端与方言分支单测覆盖 |
| CreateDump / Logger / HeartBeat alarm / db 断连 | 基础设施生命周期，单测兜底 |
| `l_*` Lua 绑定中教材未调用的部分 | 教材是教材不是测试夹具，不为测试加调用；教材该有而没有的功能另行提内容需求。**2026-11-01 内容需求批次兑现**（作者点名需求 → 教材补内容 → 剧本自然覆盖，符合本行既定通道）：① 锁箱/钥匙（山脚锁箱 + key.lua + 房管命令 lockbox/unlockbox）兑现 Box 四条（`CBox::locked`/`l_locked`/`l_unlocked`/`save_object`）；② 演武场（sample_arena 房 + sample_arena_master 教头脚本建房一条龙）兑现 FightRoom 12 条；③ 激励术收功分支兑现 `l_start_powerup`/`l_remove_powerup`（教材调用方落地，「零调用」前提已失效）+ 连带 `l_querymap_temp`、`lua_check_char`。`l_combat_attack_unarmed`/`l_combat_attack_weapon` 仍教材零调用，留认领 |
| `CReward::victim_penalty` | 需要「NPC 击杀玩家」或无凶手死亡；样本世界可战斗房间的 NPC 全部钉给唯一道，互动道没有可输的对手。**不为测试挪动资源归属**，单测兜底（`test_reward` KillerReward_NpcSlaysUser_PenaltyApplied） |
| `AutoFight` hatred/vendetta/aggressive/berserk | hatred 需「打过 NPC 后再次撞面」、aggressive 的进取型 NPC 只在副本（L3 独占进出）、vendetta 教材无人设置 `vendetta_mark`、berserk 无触发源——E2E 没有合法触发面，单测已覆盖分支（`test_auto_fight`） |
| 组队/夫妻状态拖入 PK 战斗房（`auto_follow_fight` 的两条拖入分支） | 会令入队方落入两队籍（`join_team` 无跨队去重），破坏赢家账号的确定性；单测已钉（`test_fightroom_team`），E2E 的 PK 段在组队解散后的干净状态下进行 |
| `vote` 的**通过分支**（设置/清除 `chblk/*` + 成功播报）与 `nullify_vote` 超时清除 | 通过需 ≥3 票而 `determine_total`/`counting` **每 IP 只计 1 票**（下限 3 票）——单机 E2E 全账号同 IP，结构性不可达（多机多 IP 才可达）；`nullify_vote` 是 call_out 600 拍（10 分钟）超时清除，超出 E2E 时长预算。入口/门槛/去重/未达标播报分支由 `social/vote.sce` 覆盖（含 level 门槛负测与同 IP 去重实证） |
| `mv` / `loc`（地图定位协议） | 客户端地图 HUD 的协议透传：`loc` 的核心副作用（`on_enter` + `location_object` 双向推送）在**每次登录**都被覆盖（E2E 的 ADDOBJECT 链）；`mv` 仅设 `map/*` temp + 同房广播，无分支/无拒绝路径/无状态机。无头 CLI 不发地图坐标，补发只会测「透传不崩」——低价值，记录不测 |
| `info` / `item` 命令入口 | **已确认不存在**（P1 遗留销账）：`CmdUser.h` 无声明、`CmdUser*.cpp` 无实现、`CmdUserInit.cpp` 注册表无条目——三无即无此命令，无代码可删也无入口可补；其背后的 `flush_my_info`/`flush_my_equip` 由装备/战斗等路径全覆盖（单测 95%+） |
| 9 个「Lua→C++ 显式调用」胶水绑定：`l_attempt_apprentice`、`l_create`、`l_die`、`l_do_channel`、`l_do_use`、`l_init`、`l_prevent_learn`、`l_recognize_apprentice`、`l_room_reset` | 教材从未**显式调用**这些注入函数（N1 甄别实证：检索命中全是 `function M.key(` 字段定义、注释或模块同名函数撞车）——这些能力的可走路径是 C++ 直调脚本字段/钩子（`C→Lua`），胶水方向天然无人走；0 命中与 `l_*` 教材未调用同性质（§5.3 判据陷阱已记录）。单测对对应 C++ 实现有覆盖 |
| **N2-2d 载体/周期裁定**：`l_add_balance`（**2026-11-01 已兑现**）、`l_send_state`、`l_load_from_server` | 全库事实核查（2026-10-31）：① `resource/fee` 原全库唯一设置=茶棚 **0**（`liquid_base` 记账分支 `fee>0` 永假）——**2026-11-01 内容需求批次改写**：作者点名「茶水收费」，茶棚 fee 定 10（营业场所收茶钱的世界设定），L1 剧本以「穷光蛋」负支 + 扣费成功支 + `/balance` 100→90 状态断言成对覆盖，`l_add_balance` 随之兑现，本子项作废；② `food_base` 与 `poison_base` **零继承者**（sample 食物/毒药自带实现，基类无人用）；③ 房产缴费/离线 owner 重建（house_base 112-131、`HeartBeat::look_for_objects_to_swap`）双周期卡死——房间 reset **900 秒一轮**（E2E 窗口仅启动瞬间跑过、当时房未买）且计费 `days×price/250` **按天**（单轮 days=0 直接 return）。②③仍属教材内容/调度与 E2E 时长尺度的结构性不交，非剧本可达性问题。 |
| **N2-2a lua 域甄别 34 条** | 四子类归因（2026-10-31）：① 巫师/诊断域 14（`/reload` 权限 80 命令的整条 bridge-script-notify 链、`dump_caller_info` 加载失败打印、`get_magic_name`/`display_name` **全库无调用方**、`lua_bind_check_object`/`lua_check_char` 内层助手其绑定教材未调）；② hook 读取端 5（`result_bool`/`is_number`/`int`/`table_str_*` 全挂在**未注册 hook** 后——`calculate_damage`、`can_prepare_combine` 不在注册面）；③ 教材未定义字段/4 参链 8（`greeting`/`finish_kill`/`is_defeated`/`accept_fight`/`valid_force_pro` 无字段定义；`handle_action4` 四参链教材只给 3 参版、菜单走 3 参已命中）；④ 教材零调用胶水 7（`lua_do_wield` 系与 glue 四 API——教材 grep 零调用）。与 9 个 N1 胶水绑定、`base_query_action`/`has_query_action` 回落链（教材 29 攻击技能全有 query_action 字段）同性质：**教材没用/入口不属 E2E 域**，非可达性缺口。**D 组 5 条同轮核结追认**：LuaRoom 的 `create_int_impl`/`valid_leave_impl`/`desc_impl` false_type 分支=**标签分发死支**（traits 探测 C++ 方法、E2E 房间类型不走该支）、`reset` 转发=心跳 900s 轮从未跑；`LuaNpc hit_ob` 实证=**山鸡出手 3 次仍不进**（野兽文案源自 CharFight.cpp:91 内置表 → NPC 攻击走老链不经 Attack 的 skill hit_ob 段）——**新老攻击链并存的分流记为生产侧待察项**。**C① 核结追认**：`can_combine_in_box`(inl) 需「有脚本+CAN_COMBINE+进箱」三门，E2E 箱内四物唯一过堆叠门的 coin 是无脚本特型走基类版、玩家侧无 `put` 命令、教材无投放可堆叠脚本物路径——结构性不可达（若未来加 put/战利品箱可重开） |
| `l_query_team`（`throwing.lua` do_sneak 同队检查） | **结构性不可达**（N2-2b 甄别）：同队检查（42 行）之前有 33 行 `is_user(target)`「不能偷袭玩家」的 PvE 收窄——同战斗队成员必为玩家、被 33 行拦停；唯一到达 42 行的路径是「我与目标（NPC）都在战斗中且 `query_team` 相等」，而未分队时 `query_temp` 返回 0 与 `FIGHT_TEAM_A=0` 同值（**0 歧义**）→ 0==0 意外相等误拒——此路径需「NPC 混战」无自然触发面，且补测它的是 Lua 行（不进 E2E 覆盖统计）。C++ 侧 `CChar::query_team` 本体被战斗房链覆盖（L5 fight/pk）。**0 歧义观察**（生产侧后续项，不属 E2E 批次）：`fight/fight_team` 的 0 同时是 `FIGHT_TEAM_A` 与「未设」，三个消费点受影响——`throwing.lua:42` 野外误拒、`CharState.cpp:243` 的 `if (query_temp(...)) tea=1` 让 A 队漏发战斗房标记、`CChar::is_fighting(enemy)`（仅 AutoFight 消费，§6 已排除）的普通战斗 `!=` 判定失效；处置留待战斗房语境专项评估，此处只记录不修 |
| `l_is_fighting_target`（`force_adv.lua` do_roar 狮子吼） | **内力经济结构性不可达**（N2-2b 甄别）：唯一教材调用点在 `do_roar`（CAST_IN_COMBAT + **mp≥200** 硬门槛）。而引擎内力经济有硬约束——`exercise` 收工/halt 两条退出路径都把 `mp > 2×max_mp` 压回、收工 `mp ≥ 2×max` 时抬上限 1 点并 **mp 回落 max**（`CmdStandardLife.cpp` 345/373/393），即**任何时刻 mp ≤ 2×max_mp**；max 唯一快捷来源是经验果（+100，山顶树箱 **钉 L3 独占**）→ L3 `max=101`、**mp 封顶 202** < roar 200 + sneak 100 = 300（不可共存，二选一即拿 do_roar 换掉已稳的 do_sneak 覆盖）；靠逐轮打坐抬 max 成本 ≈1 万拍；补第二段打坐编排需 **+181s → L3 521s 爆绝对预算 420**（关键路径禁加）。L2 无果实、max 天花板更低。**若未来出现内力载体**（内力药/多果实），此条可重开 |
| **L8 拍板 B（2026-10-31，2026-11-01 破障实测修订）**：`l_receive_wound`（CharCombat.cpp:395）、`l_combat_attack`/`l_combat_attack_unarmed`/`l_combat_attack_weapon`（Combatd.cpp:86/101/115） | **`l_receive_wound`——2026-11-01 破障实测（巫师 verbose 隔离实验）把拍板 B 的旧推导修正为「数学恒不可达」**：旧分析按 `mp=500 → −5+factor` 认为山贼 mp 可凑出 `damage<0`，**忽略了 force.lua:39-41 的 `mp_you ≤ 3mp` 封顶**——damage = ⌊mp/30⌋ + factor − ⌊min(mp_you,3mp)/100⌋，由 ⌊3mp/100⌋ ≤ ⌊mp/30⌋ 恒成立（0.03≤0.0333）且 factor ≥ 1（`enforce` 拒负值，CmdStandardMaster:576）→ **damage ≥ 1 恒成立**，三门中的首门（`damage<0`）任何合法取值都进不去，后两门（徒手/随机）与命中面（实测破障可达：set_skill unarmed 300 起 P(命中)≥91%）都无从谈起。故本条由「降断言等概率收割」改为**明确不测**；`force hit_ob` 的 C++ 调用点（Attack.cpp:364 三闸：mp_factor、mp≥factor、force 已激发）随 L8 战斗走 ≥0 正常分支正常执行。**重开条件（生产侧设计变更才有效）**：force.lua 的封顶或反震公式被设计修订、或出现负 mp_factor 的合法来源——均属游戏机制决策，**不为覆盖而改**。实测数据（2026-11-01 巫师 verbose 隔离实验，`env/combat=verbose` 打印 AP/DP/PP）：山贼 DP 49048.51（spawn `25+random(15)` 分布 ≈4.9万~8.4万）、PP = 2×DP（徒手攻方招架 ×2）≈9.8万~16.9万 **PP 是命中瓶颈**；`query_skill(name)` 语义 = 自身折半 + 激发技全值（CharSkill.cpp:178/181），故 `set_skill N` 生效 level = N/2，L8 镜像参（combat_exp 50000/str 10/dex 14）AP(N) = ((N/2)³/3+31000)/30×(12+N/10)，三点实测全中（存 0→15500 / 60→24000 / 200→388622）；P(命中) = AP/(AP+DP)×AP/(AP+PP)：set 200≈71%、set 300≈91%、set 400≈97%——破障可达（set 300 起）但对覆盖无新增收益。**`l_combat_attack*`**：唯一教材调用点 = `sword_adv.lua:94-98` / `throw_adv.lua:94-98` 的 `do_qi` 三连击，硬门槛 = **adv 技能 ≥60 且 mp ≥100**（throw 版 mp≥80）；E2E 各道 adv 线每条仅 1~3 次 `/learn`（`learn_sword.sce:74`、`learn_basic.sce:36`、`learn_throwing.sce:17/69/88`）远低于 60，mp≥100 又受同条 `l_is_fighting_target` 的内力经济约束；`hooks.lua:30` 的另一处只在注释里且所在 hook 未注册。**重开条件**：出现「adv 技能 ≥60 + 战斗中 mp≥100」的供给源（N4 巫师 `set_skill`/`call set` 即此），届时按实测升回确定性断言。**2026-11-01 兑现**：L9 剑气供能（`set_skill sword_adv 60` + `call set mp`）就位，L2 `skill/sword_qi.sce` 非战斗施放 do_qi（`check_ap` 对非战斗放行，L3 偷袭同款先例）命中 `l_combat_attack`；`l_combat_attack_unarmed`/`l_combat_attack_weapon` 教材仍零调用（全库 grep 证实，do_qi 只走通用 `combat_attack`），留认领 |

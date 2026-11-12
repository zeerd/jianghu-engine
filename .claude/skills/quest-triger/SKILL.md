---
name: quest-triger
description: 给定接任务的 NPC 和 数据集，分析该 NPC 的任务流程，生成只含 Triger 部分的 ini 任务脚本。用于编写或扩展 X-MUD/叛逆客户端的任务自动化脚本。
---

# 任务脚本 Triger 生成

输入：**接任务的 NPC**（中文名或英文 id）+ **数据集**。
输出：一份 **只写 Triger 部分** 的 `.ini` 脚本，其余节（Setting/PreCmd/Seek/Share 等）留空占位。

生成前必读：`references/triger-syntax.md`（Triger 结构与字段规范）、`references/dataset-guide.md`（数据集怎么查）。
输出骨架参考 `templates/skeleton.ini`。

## 工作流

### 1. 定位接任务的 NPC 脚本

按中文名反查（见 dataset-guide）：

```
grep set_name\("百晓生  dataset\npc --include=*.lua
```

拿到脚本路径后**通读整个文件**，重点是 `do_talk`、`handle_action`、以及 `askquest`/`report`/`fail` 之类的处理函数，画出任务状态机：

- **怎么接**：菜单项（`add_menu_item("斩妖除魔", "$0askquest $1")`）还是 `/ask 参数`；
- **接任务时 NPC 说什么**：`say()`/`tell_object()`/`message()` 里的原文 → 这是 `KEY=` 和第一条 `Msg=` 的素材；
- **任务目标**：杀谁（`random_npc()` 随机目标 or 固定表）、送什么（`load("item","xxx")`）、去哪个 `area`（`position_npc()` 返回的房间名）；
- **怎么交**：`/report`、`/askquest`、`!Give 物品` 等；
- **完成/失败时玩家看到的原文** → 收尾触发的 `Msg=`。

### 2. 反查目标实体

| 要查什么 | 怎么查 | 用在哪 |
|---|---|---|
| 目标 NPC 英文 id | `grep set_name("卓一行" npc --include=*.lua`，取第二参数 `zhun yi hang` | `Obj=`、`!t zhun yi hang,/kill` |
| 房间名 | `grep -F 'name = "万劫谷石屋"' room`；确认存在 | `!go 万劫谷石屋` |
| 物品中文名/ID | `grep 绝情丹 item --include=*.lua` | `!Give 绝情丹`、`[count(绝情丹)]` |

随机任务（如万劫斩妖除魔、商人任务）目标不固定，用通配符兜底：`Obj=--;`（Enemys 列表）或 `*妖怪*`。

### 3. 生成 Triger

- **一个任务分支 = 一个 `[TrigerSet_N]`**，N 从 1 连续编号；组内 `[TrigerN_M]` 的 M 从 1 递增。
- `KEY=` 写**接任务那一刻玩家会看到的原文**，变量部分用 `*`，多候选用 `|`。若配合 Seek/`!set` 流程，KEY 也可写 `你设置环境变量：任务 为 X。`（见范例）。
- 每个 TrigerSet 的**第一条 Msg 必须包含 KEY 的文字**，命令写启动动作（出发/传送）。
- 典型步骤链：**出发（!go / !t xxx,/trans）→ 途经兜底（找不到路径→换交通）→ 到达（已经到达X）→ 交互（!t id,/ask、/kill、!Give）→ 完成回执（设置下一状态变量或结束）**。
- 交互/收尾步骤加 `Tlp=1`（沿用范例惯例）；`Tmr` 交互用 500~800，走路 300~400。
- `Cmd` 与 `Obj` 按 `;` **一一对应**，数量必须对齐。

### 4. 输出

1. 按 `templates/skeleton.ini` 组织：`[Setting]` 只保留 `TrigerSetNum`/`TrigerShareNum`（≥ 实际分区数，惯例 100），其余节留空占位注释。
2. **编码必须是 GBK**（客户端按 ANSI 读，UTF-8 会全屏乱码）。用 write 工具写出后转码：

```powershell
$p='任务名.ini'; $t=[IO.File]::ReadAllText($p,[Text.Encoding]::UTF8); [IO.File]::WriteAllText((Join-Path (Split-Path $p) 'tmp.ini'),$t,(Text.Encoding)::GetEncoding(936)); Move-Item -Force (Join-Path (Split-Path $p) 'tmp.ini') $p
```

3. 输出后自查（见下）。

## 自查清单

- [ ] 每个 `[TrigerSet_N]` 都有 `KEY=` 和 `TrigerNum=`（惯例 100）
- [ ] 组内第一条 `Msg` 含 `KEY` 文字；编号连续无跳号
- [ ] `Cmd` 分号数 == `Obj` 分号数（含末尾分号）
- [ ] `Msg` 中的冒号、括号与服务器实际输出一致；变化的名字/数字已换成 `*`
- [ ] `!go` 的目的地都在数据集中存在（`name = "..."` 精确匹配）
- [ ] `Obj=`/`!t` 后的 id 是 `set_name` 第二参数原文（含空格）
- [ ] 单行 ≤ 512 字符；文件为 GBK 编码
- [ ] 已留兜底：`找不到从此处到*的路径`、`你的动作还没有完成，不能移动`、`你重新连线进入这个世界`

## 语法速查（详见 references/triger-syntax.md）

- `Msg`：`*` 通配、`|` 多选一、行首 `R` 表示纯正则（`Cmd` 行首 `R` 则不解析 `[变量]`）
- `Cmd` 常用：`!go 地名` / `!t id,/ask 参数` / `/kill` / `!Give 物品,数量` / `!if 条件,命令;` / `!has 表达式,命令;` / `!set 变量 值` / `!where` / `@goto(TrigerN_M)`
- 目标对象：`@n` 场景第 n 项、`--` Enemys 列表、`@@` Goods 列表、`#1` 战斗对面第一人、`$ID` 自己
- `Tmr=0` 表示立即执行；`Tlp=1` 标记关键交互步骤

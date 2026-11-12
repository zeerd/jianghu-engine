# 数据集查询指南

「金剑江湖」数据集：游戏内容全部是 Lua 脚本（**UTF-8**），按类型分目录。写 Triger 前用它反查任务台词、NPC id、房间名、物品名。

| 目录 | 内容 | 对脚本的意义 |
|---|---|---|
| `npc/` | NPC（`class/` 门派、`d/` 剧情任务、`pub*/` 任务大师与店铺、`monster/` 怪物、`wiz/` 巫师） | 接任务/目标 NPC 的台词、id、任务状态机 |
| `room/` | 房间（按地区分目录） | `!go` 的目的地名 |
| `item/` | 物品（`weapon/ cloth/ food/ goods/ treasure/ book/`） | `!Give`、`[count()]` 用的物品名 |
| `quest/` | 商人任务（`quest_mgr.lua` 派发、`quest_rewards.lua` 结算、`quest_data.lua` 物品池） | 任务类型与奖励文案 |
| `condition/` | 中毒/监禁等状态 | 任务附加条件 |
| `data/` | 全局配置（`city_data.lua` 等） | 城市/区域名 |

## 关键查询

### 1. 中文名 → 脚本文件（NPC / 物品）

```
grep -rn 'set_name("百晓生' dataset --include=*.lua
```

`set_name("卓一行", "zhun yi hang")`：
- 第一参数 = 屏幕显示名（Msg、`!t` 名字匹配用）
- 第二参数 = **id**（`Obj=`、`!t zhun yi hang,/kill` 用，可能含空格，原样照抄）

### 2. 房间名 → 是否存在（`!go` 目的地）

```
grep -rn 'name = "万劫谷石屋' dataset\room --include=*.lua
```

房间文件形如：

```lua
return {
    type = "room",
    name = "星宿海",
    create = function(self)
        self:set_name("星宿海")
        self:add_object("door", "星宿海天山山路2", ...)
```

`name` 必须与 `!go` 参数**完全一致**；同一地区重名房间要用上下文（`add_object("door", ...)` 链）确认是哪一个。

### 3. 任务 NPC 的任务逻辑

任务 NPC 多在 `npc/d/`（剧情链）、`npc/pub*/`（任务大师）。读文件时抓这些部分：

| 代码位置 | 提取什么 |
|---|---|
| `create()` 里的 `set_name` | 中文名 + id |
| `do_talk(self, me, ask)` | 接任务对话分支、`add_menu_item("文案", "$0命令 $1")` → `/ask` 的参数 |
| `handle_action(self, comm, me, arg)` | `askquest`/`report`/`fail` 等命令分发 |
| `say()` / `tell_object()` / `message("vision", ...)` 原文 | **Msg/KEY 素材**（`string.format` 的固定部分写死，可变部分换 `*`） |
| `me:query("xx/yy", "T")` 状态键 | 任务阶段判断（`!if` 用） |
| `load("item", "xxx")` / `carry_object("xxx")` | 任务物品 id |
| `position_npc()` 返回的 `env:name(1)` | 目标所在**房间名**（`!go` 目的地） |
| 目标 NPC 的 `die()` | 击杀后玩家看到的完成文案 → 收尾触发 `Msg` |

例（`npc/d/wanjie/wanjie_foolmaster.lua` 万劫·斩妖除魔）：

```
接任务: add_menu_item("斩妖除魔", "$0askquest $1")   →  !t wan jie,/askquest;
接单回执: "万劫对%s说道：有%s在%s%s。\n你速去除害，泽被天下百姓。"  →  KEY=万劫对*说道：有*在*。*
交任务: add_menu_item("成果报告", "$0report $1")     →  !t wan jie,/report;
失败兜底: "万劫怒道：你做事吞吞吐吐，已经错失了良机！" →  !t wan jie,/fail;
```

### 4. 目标是随机 NPC（`random_npc()`）

目标不固定（万劫斩妖除魔、商人任务 `quest/`），脚本里查不到具体 id，处理方式：

- 出发后用通配 `Obj=--;`（配合 `[Enemys]` 列表）或 `Obj=-*妖怪*;`
- 到达判定用房间名通配：`Msg=已经到达*|你进入了*`
- 交任务步骤用固定 id：`!t wan jie,/report;`

### 5. 任务链与奖励文案

`quest/quest_rewards.lua`：`$HIC恭喜你完成了「乌海江湖」的第%d个任务，获得了%d点经验。`
`npc/d/wanjie/wanjie_foolmaster.lua` `reward_exp()`：`你获得了%s点经验、%d点江湖声望！`
→ 都是收尾触发 `Msg=` 的素材，数字换 `*`。

## 注意

- 数据集是「金剑江湖」，范例 ini 面向另一套服务器（叛逆），台词/房间可能对不上。**以数据集为准生成，再用实际游戏回显校正 Msg。**
- 数据集里没有的房间/物品，宁可留兜底触发（`找不到从此处到*的路径`）也不要编造名字。
- 数据集文件一律 UTF-8；生成的 ini 一律 GBK，两者不可混。

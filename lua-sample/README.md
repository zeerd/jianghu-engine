# lua-sample 示例项目

一个自包含的 Lua 示例项目，展示游戏脚本框架的完整能力。

## 故事概要

一座青云山，包含山脚、半山腰、山顶三个场景。山顶可以进入一个山寨副本。常规地图放置一些箱子和可聊天NPC，山寨副本放置战斗NPC（可自动出绝招）。

## 技能体系

仅放置基础和高级的内功、拳脚、格挡、轻功、剑法，各一个基础和高级：

| 技能 | 文件 | 说明 |
|------|------|------|
| 基本内功 | `skill/force.lua` | 无绝招 |
| 高级内功 | `skill/force_adv.lua` | 自疗术、狮子吼 |
| 基本拳脚 | `skill/unarmed.lua` | 无绝招 |
| 高级拳脚 | `skill/unarmed_adv.lua` | 连环击 |
| 基本格挡 | `skill/parry.lua` | 无绝招 |
| 高级格挡 | `skill/parry_adv.lua` | 卸力 |
| 基本轻功 | `skill/dodge.lua` | 无绝招 |
| 高级轻功 | `skill/dodge_adv.lua` | 凌空虚渡 |
| 基本剑法 | `skill/sword.lua` | 无绝招 |
| 高级剑法 | `skill/sword_adv.lua` | 剑气 |

## 共享模块（require）

示例项目演示了两种 `require` 用法，与真实代码库的模式一致：

### 房间公共模块

`room/sample_room_common.lua` 提供通用的 `say_long` 和 `do_look` 函数，三个山脚房间通过 `require` 复用：

```lua
local common = require("room.sample_room_common")
-- 简单房间直接复用 do_look：
return common.do_look(self, me)
-- 需要追加菜单的房间只复用 say_long：
common.say_long(self, me)
self:AddMenuItem("闯入山寨", "$0enter_fb $1", me)
self:SendMenu(me)
```

参照真实代码库的 `room.changan.changan_room_common`（100+ 个房间共用）。

### 物品公共模块

`item/sample_book_common.lua` 提供数据驱动的秘籍创建函数，两个秘籍通过 `require` 复用：

```lua
local book = require("item.sample_book_common")
-- 在 create 中调用：
book.create(self, nFlag, { name = "内功秘籍", skill_name = "force", ... })
```

### 共享模块文件

| 模块 | 文件 | 说明 |
|------|------|------|
| 房间公共 | `room/sample_room_common.lua` | 通用 do_look |
| 秘籍公共 | `item/sample_book_common.lua` | 数据驱动创建 |

## 物品体系

箱子和商店放置以下物品：

| 物品 | 文件 | 说明 |
|------|------|------|
| 铁剑 | `item/sample_sword_basic.lua` | 基础武器 |
| 青锋剑 | `item/sample_sword_adv.lua` | 高级武器 |
| 内功秘籍 | `item/sample_book_force.lua` | 学习内功（使用 book_common） |
| 剑法秘籍 | `item/sample_book_sword.lua` | 学习剑法（使用 book_common） |
| 金创药 | `item/sample_heal.lua` | 回血药品 |
| 断肠散 | `item/sample_poison.lua` | 扣血毒药 |
| 传送符 | `item/sample_teleport.lua` | 弹出菜单选择目的地传送 |
| 茶壶 | `item/sample_teapot.lua` | liquid 容器，可 fill 装茶、drink 饮用 |

## NPC体系

### 山脚

| NPC | 文件 | 说明 |
|-----|------|------|
| 老李头（向导） | `npc/sample_guide.lua` | 聊天NPC，随机自言自语 |
| 杂货老板（商人） | `npc/sample_vendor.lua` | 出售所有物品 |
| 茶倌（商人） | `npc/sample_tea_vendor.lua` | 出售茶壶（liquid 物品） |

### 茶棚（山脚分支）

| 场景 | 文件 | 说明 |
|------|------|------|
| 山脚茶棚 | `room/sample_tea_stall.lua` | 有水源（resource/water），可 fill 装茶 |

### 半山腰

| NPC | 文件 | 说明 |
|-----|------|------|
| 青云子（师父） | `npc/sample_master.lua` | 门派"青云门"祖师，可拜师学武 |

**师父机制**（参照 `lua/npc/class` 下的NPC）：
- 使用 `create_family()` 创建门派
- 使用 `recognize_apprentice()` 判断收徒条件
- 使用 `DownLoad()` 自动教授师父会的技能
- 师父会什么就教什么，不使用预定义列表

### 副本（黑风寨）

| NPC | 文件 | 说明 |
|-----|------|------|
| 小喽啰 | `npc/sample_bandit_low.lua` | 战斗NPC，弱 |
| 大当家 | `npc/sample_bandit_boss.lua` | 战斗NPC，自动出绝招 |

**战斗NPC机制**（参照 `lua/npc/pub/pub_wokou1.lua`）：
- `chat_chance_combat` 控制战斗中自动出招概率
- `chat_msg_combat()` 中调用 `perform_action()` 出绝招

## 场景结构

```
山脚 ──→ 半山腰 ──→ 山顶 ──→ [副本]黑风寨大厅
 │   │                │                    │
 │   └→ 茶棚          │                    │
 │     (水源fill/商人)  │                    │
 向导+商人            师父（学技能）        小喽啰+大当家（战斗）
 箱子(剑/药)          自动出绝招
```

### 文件列表

| 场景 | 文件 | 说明 |
|------|------|------|
| 房间公共模块 | `room/sample_room_common.lua` | require 共享 do_look |
| 山脚 | `room/sample_mountain_foot.lua` | 向导+商人，箱子 |
| 山脚茶棚 | `room/sample_tea_stall.lua` | 水源fill，茶倌商人 |
| 半山腰 | `room/sample_mountain_mid.lua` | 师父NPC |
| 山顶 | `room/sample_mountain_top.lua` | 副本入口 |
| 副本总控 | `room/sample_stronghold_fb.lua` | type=fb |
| 副本房间 | `room/sample_stronghold.lua` | type=fbroom，战斗NPC |

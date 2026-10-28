# lua-sample 示例项目

一个自包含的 Lua 示例项目，展示游戏脚本框架的完整能力。

## 故事概要

一座青云山，包含山脚、半山腰、山顶三个场景。山顶可以进入一个山寨副本。常规地图放置一些箱子和可聊天NPC，山寨副本放置战斗NPC（可自动出绝招）。

## 技能体系

示例只自带**高级**技能脚本；**基础**技能（内功、拳脚、格挡、轻功、剑法）
由服务器默认脚本目录 `lua-default/skill/basic` 提供。本目录的高级技能：

| 技能 | 文件 | 说明 |
|------|------|------|
| 高级内功 | `skill/force_adv.lua` | 自疗术、狮子吼 |
| 高级拳脚 | `skill/unarmed_adv.lua` | 连环击 |
| 高级格挡 | `skill/parry_adv.lua` | 卸力 |
| 高级轻功 | `skill/dodge_adv.lua` | 凌空虚渡 |
| 高级剑法 | `skill/sword_adv.lua` | 剑气 |
| 高级投掷 | `skill/throw_adv.lua` | 飞星连珠；`valid_learn` 要求手持暗器 |

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
| 金创药 | `item/sample_heal.lua` | 回血药品（`receive_curing`） |
| 回春丹 | `item/sample_hp.lua` | 回血药品（`receive_healing`） |
| 经验果 | `item/sample_xp.lua` | 使用后增加战斗经验 100000、内力上限 +100（山顶随机树箱掉落） |
| 断肠散 | `item/sample_poison.lua` | 剧毒：服下即扣气血与气血上限，并挂载「断肠散之毒」状态持续扣血 |
| 传送符 | `item/sample_teleport.lua` | 弹出菜单选择目的地传送 |
| 茶壶 | `item/sample_teapot.lua` | liquid 容器，可 fill 装茶、use 饮用 |

## 状态体系（condition）

「状态」是挂在角色身上的持续效果，由 `condition/` 目录下的脚本定义。
**条件名即脚本名**：`apply_condition("<条件名>", <拍数>)` 会加载
`condition/<条件名>.lua`，脚本须返回 table：

| 脚本字段 | 何时被调用 | 约定 |
|----------|------------|------|
| `init(self, me)` | 状态挂载瞬间调用一次 | 播报进入状态；抛错则状态挂不上 |
| `update_condition(self, me, duration)` | 每次心跳调用一次 | 返回 `CND_CONTINUE, duration - 1` 继续倒数；返回 `0` 解除状态 |

`duration` 的单位是**心跳拍数**而不是秒——角色心跳约每 5~9 秒一拍。

| 状态 | 文件 | 说明 |
|------|------|------|
| 断肠散之毒 | `condition/sample_poison.lua` | 断肠散服用后挂载：每拍扣 5% 气血上限，持续 2 拍后毒性散尽 |

## 房屋体系（住宅区 + 私宅）

`room/sample_resident.lua` 演示引擎的**居民区**房间：脚本声明
`type = "resident"` 即完成装配（引擎在脚本加载期自动混入
`room/resident_base.lua` 的通用逻辑，无需 require + `install`），玩家在住宅区
申请成为本城居民，缴银登记房屋后即可「回家」进入自己的宅院。

| 场景 | 文件 | 说明 |
|------|------|------|
| 青云城住宅区 | `room/sample_resident.lua` | `type = "resident"`（自动装配属区逻辑）；`resident="青云"` 是城名；山脚有门可进；驻守月老（`sample_marriage`）演示结婚/离婚 |
| 宅院仆役 | `npc/monster_yongren.lua` | 引擎建房时按固定基名 `monster_yongren` 载入，替房主看家拦人 |
| 月老（结婚登记） | `npc/sample_marriage.lua` | 住宅区驻守，演示结婚/离婚全流程（费用 100 金） |


**房屋逻辑通用、玩家不写脚本**：房间名以 `house_` 开头即由引擎建成前缀
动态房，加载 `room/house_base.lua`（租借品看管与外人指令截获拆在其子模块
`room/house_rent.lua`），含一个「柜子」、一个仆役、一扇通回住宅区的门，
房屋描述与登记由 `room/resident_base.lua` 维护（`registerhouse` →
`update_register` → `set_house`）。

## NPC体系

### 山脚

| NPC | 文件 | 说明 |
|-----|------|------|
| 老李头（向导） | `npc/sample_guide.lua` | 聊天NPC，随机自言自语；在山顶随机刷出"奇怪的树"（树箱）；身兼任务发起人（`data/quest.lua`） |
| 杂货老板（商人） | `npc/sample_vendor.lua` | 出售武器、秘籍、药品、茶壶、棋子等；`/ask 交易` 菜单索取交易文书 |
| 月老（结婚登记） | `npc/sample_marriage.lua` | 住宅区驻守，演示结婚/离婚全流程（费用 100 金） |

### 茶棚（山脚分支）

| 场景 | 文件 | 说明 |
|------|------|------|
| 山脚茶棚 | `room/sample_tea_stall.lua` | 有水源（resource/water），可 fill 装茶 |

### 半山腰

| NPC | 文件 | 说明 |
|-----|------|------|
| 青云子（师父） | `npc/sample_master.lua` | 门派"青云门"祖师，可拜师学武 |
| 练功木人 | `npc/sample_wooden_dummy.lua` | `type = "mirror"` 镜像NPC：`/ask 对练` 照挑战者复制自己，被打散即自毁、随房间 reset 复生 |

**师父机制**（参照 `lua/npc/class` 下的NPC）：
- 使用 `create_family()` 创建门派
- 使用 `recognize_apprentice()` 判断收徒条件
- 使用 `DownLoad()` 自动教授师父会的技能
- 师父会什么就教什么，不使用预定义列表

**镜像NPC机制**（基类 `src/npc/NpcMirror.h`，类型分派 `src/engine/SetupObjects.cpp`）：
- NPC 脚本声明 `type = "mirror"`，引擎即按 `CLuaMirrorNpc`（`CNpcMirror` 基类）创建
- `self:make_mirror(me, diff)` 把 `me` 的技能、技能映射、激发、先天属性、气血内力
  与历练等级整套按 `diff / 10` 抄到 NPC 身上，并照抄来的技击技能配一把「木刺」——
  木人的「对练」由此而来：`/t 练功木人` + `/ask 对练`，它当场变成「八成的你」
- `make_mirror` 末尾按 `me` 的 `fool/name` 给 NPC 改名，玩家没有这个临时字段时
  写进来的是空串，所以脚本抄完必须把自己的名字写回去
- 木人不设 `chat_chance`（避开基类 `chat_msg` 的概率遁走自毁），并自定义 `die`
  把「化为灰烬」换成散成一地木头；死亡不走 `die_base`，故不掉尸体、不结算 NK 经验

### 山顶

| NPC | 文件 | 说明 |
|-----|------|------|
| 山鸡 | `npc/sample_pheasant.lua` | 1级练级怪，不会还手；击杀经 `hooks.kill_bonus` 奖励 10000 经验 |

**练级奖励机制**（挂点见 `data/hooks.lua`，钩子契约见 `lua-default/data/hooks.lua`）：
- `hooks.kill_bonus(killer, victim, reward)` 在击杀 NPC 经验封顶后、入账前被调用；
  每次玩家击杀都会咨询（reward 可能为 0），因此固定奖励不因 nk_gain 的
  组队分红、晕倒归属、经验折算等上游因素失效
- 山鸡（`sample_pheasant`）返回 `{ reward = 10000 }` 覆盖入账值，其余 NPC 返回 nil 按常规入账
- 击杀后山鸡随房间 reset 自动补充，可持续练级

**任务机制**（`data/quest.lua`，钩子契约见 `lua-default/data/hooks.lua`）：
- 引擎不认任何「任务」类型，任务是纯数据集逻辑，只挂在两个通用挂点上：
  `hooks.pre_talk(npc, me, ask)` 每次对话咨询一次，
  `hooks.pre_accept_object(npc, me, item)` 每次把东西交给 NPC 咨询一次
- 返回 `{ handled = true }` = 本次由脚本接管（引擎不再走自己的询问菜单 /
  默认拒收）；返回 `nil` = 不适用，引擎照常走自身流程
- 本示例是老李头的一桩小差事：`/ask 委托` 接下 → 交给他要的金创药
  （交物被钩子接管）→ `/ask 复命` 领赏；任务状态记在角色 dbase 的
  `quest/lilaotou` 上，随角色存档走

**其余挂点**（同样见 `data/hooks.lua`）：
- `data/chanbanword.lua`：频道禁词表（可选数据集配置），命中即拒发；
  不提供本文件时按空词表处理

**内力上限与经验果**：引擎只为 NPC 推导 `max_mp`，玩家的内力上限只能靠打坐
每完成一轮涨 1 点。经验果吃下后除历练外还拓宽经脉（`max_mp +100`），
但**上限抬了当前内力不会自己跟上来**——引擎始终把内力压在 `max_mp` 之下，
吃完还得打坐一阵把内力真练进去（道理写在 `item/sample_xp.lua` 的注释里）。

**随机事件（概率性内容）**：
- 山脚的向导老李头挂一个延迟函数，以 1/10 的概率在山顶刷出"一个奇怪的树"
  （`Add_Box` + `sample_xp` 经验果）；未命中则 5 秒后重掷，直到刷出为止
- 演示 `random()` + `call_out()` 驱动的概率事件与按需刷内容

### 副本（黑风寨）

| NPC | 文件 | 说明 |
|-----|------|------|
| 小喽啰 | `npc/sample_bandit_low.lua` | 战斗NPC，弱 |
| 大当家 | `npc/sample_bandit_boss.lua` | 战斗NPC，自动出绝招 |

**战斗NPC机制**（参照 `lua/npc/pub/pub_wokou1.lua`）：
- `chat_chance_combat` 控制战斗中自动出招概率
- `chat_msg_combat()` 中调用 `perform_action()` 出绝招

**副本门机制**（参照 `src/room/FBRoom.cpp` 的 `CFBDoor`）：
- 副本房间内 `add_door(出口名, 目标房, 目标房出口名)` 创建的是副本出口：进入时经父副本 `load_fb_room` 按需加载目标房（不查全局房间表），两房同属一个副本实例
- 副本总控只预加载入口"黑风寨庭院"，"黑风寨大厅"由庭院的副本门首次通过时按需创建
- 大厅回庭院的出口在敌人清场前被 `valid_leave` 拦截，清场后可原路返回

## 场景结构

```
山脚 ──→ 半山腰 ──→ 山顶 ──→ [副本]黑风寨庭院 ──→ 黑风寨大厅
 │   │                │               │              │
 │   └→ 茶棚          │               │              │
 │     (水源fill)      │               │              │
 向导+商人            师父（学技能）    守门喽啰        小喽啰+大当家（战斗）
 箱子(剑/币/药)        自动出绝招     (副本门→大厅)
                       练功木人(对练)
 随机树箱(经验果)
 └→ 住宅区（申请成为居民 → 置产 → 搬进自家宅院）
```

### 文件列表

| 场景 | 文件 | 说明 |
|------|------|------|
| 房间公共模块 | `room/sample_room_common.lua` | require 共享 do_look |
| 状态脚本 | `condition/sample_poison.lua` | 「断肠散之毒」状态：挂载播报 + 逐拍倒数 + 归零解除 |
| 山脚 | `room/sample_mountain_foot.lua` | 向导+商人，箱子 |
| 山脚茶棚 | `room/sample_tea_stall.lua` | 水源fill |
| 山脚钱庄 | `room/sample_bank.lua` | 钱庄老板（存取款/贷款） |
| 住宅区 | `room/sample_resident.lua` | type=resident，申请成为居民 → 置产 → 搬家；驻守月老 |
| 宅院仆役 | `npc/monster_yongren.lua` | 房屋自带仆役，基名固定为 monster_yongren |
| 月老（结婚登记） | `npc/sample_marriage.lua` | 住宅区驻守，演示结婚/离婚全流程（费用 100 金） |
| 半山腰 | `room/sample_mountain_mid.lua` | 师父NPC + 练功木人 |
| 练功木人 | `npc/sample_wooden_dummy.lua` | type=mirror 镜像NPC：`/ask 对练` 照挑战者复制自己 |
| 山顶 | `room/sample_mountain_top.lua` | 副本入口，山鸡练级点 |
| 山鸡 | `npc/sample_pheasant.lua` | 1级练级怪，击杀奖 10000 经验（hooks.kill_bonus） |
| sample 挂点 | `data/hooks.lua` | script_root 的 hooks 实现（kill_bonus + 称谓/任务规则安装） |
| 任务 | `data/quest.lua` | 老李头的委托：pre_talk 接委托/复命 + pre_accept_object 交物 |
| 频道禁词表 | `data/chanbanword.lua` | 可选数据集配置：命中即拒发（不提供则按空词表） |
| 副本总控 | `room/sample_stronghold_fb.lua` | type=fb，只预加载入口房间 |
| 副本房间 | `room/sample_stronghold_courtyard.lua` | type=fbroom，入口+副本门 |
| 副本房间 | `room/sample_stronghold.lua` | type=fbroom，大厅+战斗NPC |

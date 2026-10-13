---
name: naming-convention
description: 全工程 C++ / Lua 命名规范（新增与修改代码必须遵守；存量改到即改对）
applyTo: '**/*.cpp,**/*.h,**/*.lua'
---

# 命名规范

本规范适用于**所有新增代码**与**任何被修改的符号**。存量代码遗留的旧风格不主动迁移；
在存量代码中碰到旧风格时，**不要**为了"和周围旧代码保持一致"而沿用错误风格——改到哪个符号，就把那个符号改对。

本规范描述名字**应当长成什么样**，不是清点工程里现有的名字：正例是必须写出的形态，
反例是禁止写出的形态（其中可能含工程现存的遗留违规名——遇到即改对，但不主动迁移）。

---

## 1. C++ 标识符

**总则**：类型（类 / 结构体 / 接口 / 枚举 / 类型别名）用 `PascalCase`；函数、方法、变量、参数一律
`snake_case`；编译期常量与宏用 `UPPER_SNAKE`；带前缀的族（`m_`/`s_`/`g_`/`l_`）在前缀之后接 `snake_case`。
任何名字都不得混用两种风格；逐类的例外与细分约定见下表。

| 类别 | 规则 | 正例 | 反例 |
|:---|:---|:---|:---|
| 类 / 结构体 | `PascalCase`；**类**带 `C` 前缀，**struct 不带前缀** | `CChar`、`CContainer`、`AttackContext`、`DbRow` | `char`、`CombatConfig`、`C_AttackCtx` |
| 接口 | `I` + `PascalCase` | `IDatabase`、`IHittable`、`IClientObject` | `Hittable` |
| 模板包装类 | `C` + 基类名 + `T` | `CLuaNpcT`、`CLuaItemT`、`CLuaRoomT` | `LuaNpcTemplate` |
| 枚举类型 | `PascalCase`；优先 `enum class` | `enum class RecvAction`、`LuaScriptType` | `enum e_recv_action` |
| 枚举成员（`enum class`，纯 C++ 内部） | `PascalCase`（限定名天然消歧） | `WorkerAction::SendDone`、`RecvAction::Rearm` | `WORKER_ACTION_SEND_DONE` |
| 枚举成员（传统 `enum`，值泄漏到外层作用域） | `UPPER_SNAKE` | `DO_TALK`、`COMBAT_TYPE_QUICK` | `young_monk`、`cnd_continue` |
| 枚举成员（暴露给 Lua） | `UPPER_SNAKE`；名字是 Lua 脚本按字符串引用的**协议，不可改** | `IS_CORPSE`、`CAN_SAVE` | — |
| 方法 / 自由函数 | **`snake_case`**（唯一风格，禁止 PascalCase 与混合） | `query_weight`、`handle_action`、`on_enter`、`remove_npc` | `Query_Weight`、`Do_Action`、`OnEnter`、`Clean_Up` |
| Lua C 闭包 | `l_` + `snake_case`（**例外**，固定前缀） | `l_set`、`l_catch_ob`、`l_log_trace` | `L_Set`、`set_binding` |
| 成员变量 | `m_` + `snake_case`，**禁止类型前缀字母** | `m_tick`、`m_amount`、`m_equip_list` | `m_nTick`、`m_lAmount`、`m_EquipList` |
| 静态成员 | `s_` + `snake_case` | `s_current_level`、`s_run_id` | `s_Level`、`g_level` |
| 全局变量 | `g_` + `snake_case`（**唯一**全局前缀，禁用 `sys_` 与裸名） | `g_combatd`、`g_server_id`、`g_current_time` | `g_Combatd`、`sys_server_id`、`current_time`、`g_bReboot` |
| 形参 / 局部变量 | `snake_case`，**禁止 `n`/`p`/`l`/`b`/`f`/`x` 等匈牙利前缀** | `attr`、`me`、`amount`、`ulist` | `nAttr`、`pMe`、`lValue`、`bFight` |
| 常量 / 宏 | `UPPER_SNAKE`，禁止前导下划线 | `MAX_ENV_VARS`、`IDLE_TIMEOUT` | `Main_VERSION`、`_NUM_OF_MAXUSER`、`_RUNGATE_STATUS_GOOD` |
| 类型别名 `typedef` | `PascalCase` | `DTItemList`、`TeamList` | `MAP2USER`（现存遗留）、`map2user`、`DT_itemlist` |

**语义前缀（推荐，非强制）**：布尔查询用 `is_`/`can_`/`has_`（`is_fighting`、`can_equipped`）；
dbase 读写用 `query_`/`set_`/`del_`（`query_exp`、`set_title`）；生命周期回调用 `on_`（`on_enter`、`on_moved`）。

**参数顺序**：函数参数中同时存在输入参数与输出参数时，**输入在前、输出在后**（如
`db_load_object(const char* owner_id, long grid, std::string& out_base_name, std::string& out_data)`）。

**特例**：`g_LuaEngine`（`#define g_LuaEngine CLuaEngine::instance()`，单例访问器宏）刻意保持
`g_` + PascalCase 形态以便与 `g_combatd` 等用法一致（`g_LuaEngine->init(...)`），不改为 `UPPER_SNAKE`。

### 命名冲突与双 API 处理

- **多对一合并**禁用同名同签名（会重复定义），用 `_impl` 后缀区分：
  `save`/`save_impl`、`load`/`load_impl`、`lock_data`/`lock_data_impl`、`restore_data`/`restore_data_table`。
- **大小写双 API**：同一类里若出现仅大小写不同的两个名字（如 `Present` 与 `present`），必须合并到
  `snake_case` 的那一个、重载靠签名区分——`CContainer::present(const char* id, ...)` 与
  `present(int32_t obj, ...)` 即合并后同名共存的形态；
  同继承链不同语义**不可合并**——`CContainer::query_attr(int)` 属性查询与继承自 `CObject` 的 `query(const char*)`
  字符串查询必须靠语义区分名分开；合并成同名会触发 C++ 名字隐藏、静默破坏多态。
- **成员名隐藏同名自由函数**：成员体内调用自由函数需全局限定 `::func(...)`（如 `CCmdUser::show_cards(...)`
  内调 `::show_cards(me)`）。
- **参数顺序相反的重载**必须改用语义区分名，禁止只靠参数顺序区分调用：如 `message_vision(msg, me, ...)`
  与 `message_vision_room(me, room, msg, ...)`，后者不得与前者同名。
- **`l_` 前缀是 Lua C 闭包专用约定**，成员方法不得 snake 成 `l_save`，改用 `save_local`。
- **Win32/第三方 API 镜像名保持原样**，不 snake 化：`WSAGetLastError`、`WSARecv`、`DispatchMessage` 等。

---

## 2. 文件名与目录

| 规则 | 说明 |
|:---|:---|
| 文件 | `PascalCase`，且**与文件内主类同名去掉 `C` 前缀** |
| 主类判定 | 一个 `.h` 只应有**一个**主类；辅助类/struct 跟随主类文件，不再单独成文 |
| 头/源配对 | `Foo.h` ↔ `Foo.cpp`；一个 `.h` 的实现可拆到 `FooBar.cpp`、`FooBaz.cpp`（按职责），但**首字母段必须与头文件一致** |
| 测试文件 | `test_<被测模块>.cpp`，保持 `snake_case`（Google Test 约定，属规范的一部分） |
| 目录 | 全小写 `snake_case`（`src/`、`lua-sample/`、`tests/mock/`） |

对照示例：`CChar` → `Char.h/cpp`；`CCreateDump` → `CreateDump.h/cpp`；`CResident` → `Resident.h/cpp`；
`IDatabase` → `IDatabase.h`；`CFBRegistry` → `FBRegistry.h`。

---

## 3. Lua 脚本

Lua 侧统一使用 `snake_case`。本节是对该约定的**固化**，同样只约束新增与被修改的脚本，不做存量风格迁移。

| 类别 | 规则 | 正例 |
|:---|:---|:---|
| 文件 / 模块名 | `snake_case`，与对象 `base_name` 一致 | `sample_master.lua`、`sample_tea_stall.lua` |
| 函数 | `snake_case` | `function npc.create(self)`、`update_condition(self, me, duration)` |
| 模块表 | 固定用 `M`（工具/公共模块）或对象类型名 | `local M = {}`、`local npc = {}` |
| 局部变量 | `snake_case` | `local target = nil` |
| 常量 | `UPPER_SNAKE` | `local MAX_LEVEL = 176` |
| 私有 | 前导 `_`，不导出到模块外 | `function M._xrandom(v)` |

**禁止**：Lua 里出现 PascalCase 或 camelCase 函数名。

**对象脚本文件名**：对象脚本的文件名即游戏数据键（房间/NPC/物品加载名、存档引用），**必须**与脚本
声明的 `base_name` 一致且为 `snake_case`——如 `sample_stronghold.lua`、`sample_bandit_boss.lua`、
`sample_tea_stall.lua`。纯库模块由 `require` 引入、不作数据键，同样须 `snake_case`——
如 `sample_room_common.lua`、`sample_book_common.lua`。

**绑定键同名函数**：Lua 中与 C++ 绑定表键同名的函数（如 `Query`，对应注册键 `{"Query", l_query}`）保持原名。

# Bind 精简计划书（bind-reduction）

> 落盘位置：执行阶段第一步搬进 `docs/plans/bind-reduction-PLAN.md`（仓库计划书惯例：
> 只放待办条目 + 勾选 + 一行运行记录，用完即删；不得被 design/规则/剧本引用）。
> 本文件是**任务清单**，机制与理由不写在这里，见 `docs/design/engine-core-architecture.md`
> §3 分工铁律、`docs/plans/integration-test-adjudications.md` §3 胶水绑定甄别口径。

## 目标与基线

减少 C++ 侧 Lua 绑定数量，降低脚本开发者的学习难度。基线（2026-11-11 扫描，B0 对账脚本须复现同数）：

- 注册条目 **277**（`object_methods[]` 188 + `global_funcs[]` 89）
- 不同 Lua 名字 **270**（`room_exits` 两表各注册一次）
- 不同 `l_*` 实现 **264**
- 实现体 ≤19 行 **201/277 ≈ 73%**
- 严格口径（去注释、去 `function` 定义后的 `[.:]name(`）调用点：
  0 调用 **3**（`log_trace` / `log_fatal` / `prevent_learn`）、≤1 调用 24、≤3 调用 61、≤10 调用 107
- 扫描面：`c:\jianghu-dataset` 5216 + `lua-sample` 66 + `lua-default` 48 = **5330 文件 / 350,904 行**
- 实现体分布：264 个 `l_*` 散在 51 个源文件（`ContainerDbase.cpp` 21、`CharSkill.cpp` 18、`CharAttr.cpp` 15、`LuaBindGlue.cpp` 14 …）

**目标总量：264 → 约 192（−27%）**，其中 B14 胶水甄别全清可到 ~186。

**作用域（已裁定）**：三处调用点都改 —— `c:\jianghu-dataset`、`lua-sample`、`lua-default`。
**机制（已裁定）**：接受新增「启动期 Lua 前导脚本（prelude）」引擎改动。

---

## 硬约束（每批都适用，不逐批复述）

1. 分工铁律：C++ = 引擎与热路径，Lua = 内容与规则；全局绑定不做 Lua→C++→Lua 纯转发。
   「下沉」= 逻辑搬到内容层，不是再包一层。
2. 注释只写机制归属；**禁止**出现「为减少 bind 数量」「测试要的」这类动机注释。
3. 每改 `.cpp/.h` 必须同步改 `tests/unit/test_bind_*.cpp`（16 个文件挂钩），`delta ≥ 0`。
4. `lua-sample` 是教材、`lua-default` 是生产内容：改功能逻辑需授权，改名类按本计划执行。
5. 每批收口命令：`.\run-coverage.ps1` → `.\run-cppcheck.ps1` → `--check-lua --dir lua-sample`
   / `--dir lua-default` → `c:\jianghu-dataset\validate.ps1` → `.\run-integration-tests.ps1`。
6. 文档同步（B15 统一收口，各批不必逐个改）：`docs/e2e-unhit-audit.md §1`（73 行台账）、
   `docs/design/engine-core-architecture.md`（「~280 个 l_*」）、
   `docs/design/concepts/jianghu-engine.md:34`、`docs/design/integration-test.md §5/§6`。

---

## B0 对账工具 —— 工作量 M

- [x] 新增仓库根 `check-binds.ps1`（与 `style.ps1` / `run-cppcheck.ps1` 并列）：
  - [x] 解析 `src/lua/LuaBind.cpp` 两张表 → 条目 / 名字 / 函数三口径
  - [x] 四方对账：表项 ↔ `LuaBindDecls.h` ↔ `int l_*(lua_State` 实现 ↔ `test_bind_*.cpp` 引用
  - [x] 严格口径调用点报表（去注释行、去 `function name(`），输出到 `test_tmp/bind_usage.txt`
  - [x] 三类告警：完全重复登记、同函数多名、PascalCase 方法名
- [x] 三份 CSV 基线归档到 `test_tmp/`（`bind_usage2.csv` / `bind_strict.csv` / `bind_full.csv` 的生成逻辑并入本脚本）
- 验收：当前 HEAD 跑出 `277 / 270 / 264`、0 调用 3 个，与本计划书基线一致
- 运行记录：2026-11-11 `.\check-binds.ps1` → 277(188+89) / 270 / 264；四方对账 OK；重复 6、别名 6、PascalCase 42；0 调用 3 = log_trace/log_fatal/prevent_learn；buckets(264 函数) 0=3 ≤1=24 ≤3=61 ≤10=108

## B1 删重复登记 + 删死码 —— 工作量 S（调用点改动 0）

- [x] 删 6 条完全重复登记：`querymap_temp`、`querymap_temp_keys`、`object_id`、`set_beast_verbs`（方法表）、`load_npc`、`load_room`（全局表）
- [x] 删 `log_trace`、`log_fatal`：`LuaBind.cpp` 两行 + `LuaBindDecls.h` 两行 + `Logger.cpp:179,184` + `test_bind_global.cpp:105,127` + `test_log_levels.cpp:46-47` 名单（`test_log_levels` 保留其余 4 级）
- [x] `prevent_learn` **不动**（归 B14；收口扫描确认仍注册、0 调用）
- 改动面：5 文件，0 调用点
- 验收：全链五命令；`check-binds.ps1` 重复告警清零
- 净减：−2 函数 / −2 名字 / −8 条目
- 运行记录：2026-11-11 check-binds 269/268/262（重复告警 0、四方对账 OK）；`run-coverage` 2235+282 全过 delta +0.0%；`run-cppcheck` 无问题；`--check-lua` sample 70/default 48 全 OK；dataset validate 的 check-lua 5216 全 OK（roomcheck 74 未解析为 dataset 暂存内容的基线红，与本批无关）；集测 10 道+persist+reload 全 PASS（235s/420s，未登记 ERROR 0）
- 附带纠正（应用户指正）：查脚本的 `--check-lua` 默认 exe 由过期的 `bin\jianghu.exe` 全部改为 `bin\e2e\jianghu.exe`（集测每轮重建的专用服）——已改 `copilot-instructions.md` 调用命令与编译节、`run-integration-tests.ps1` 头注/-SkipBuild 注、`docs/design/integration-test.md` §1.7/-SkipBuild 行与 §5.1 `--modules` 行、dataset `validate.ps1`（exe 路径 + 姿势 + .NOTES）

## B2 prelude 机制落地 —— 工作量 M（调用点改动 0）

- [x] `lua_bind_init()` 末尾加载内容层前导脚本（默认根 `lua-default`，dataset 可同名覆盖）：
  - [x] 以 `jianghu_object` metatable 为唯一入参 `pcall` 执行，脚本内 `local index = ...` 拿到 `__index` 表
  - [x] `--check-lua` 路径同源加载（它本就跑完整绑定环境，`LuaSyntaxChecker.cpp:809` 动态收集 metatable 键 → prelude 定义的方法/全局自动被静态检查认得）
- [x] 前导脚本本体放 `lua-default/data/prelude.lua`（内容层，符合「全局绑定只暴露引擎能力」）
- [x] 单测：新增 prelude 加载与方法可见性用例（挂 `tests/unit/`，走 `run-coverage.ps1`）
- 验收：`--check-lua --dir lua-sample` / `--dir lua-default` 全 OK；集测全绿
- 运行记录：2026-11-11 `run-coverage` 2241+282 全过、delta +0.1%（LuaBind.cpp 65/65=100%，prelude 装载各分支全覆盖）；`run-cppcheck` 无问题；`--check-lua` sample 70 / default 49（含 prelude.lua）全 OK；dataset check-lua 5216 全 OK（roomcheck 基线红与 B1 同数）；集测 10 道+persist+reload 全 PASS（232s/420s，未登记 ERROR 0）

## B3 下沉① 日志 6→1 —— 工作量 S（调用点改动 0）

- [x] C++ 只留 `log(level, fmt, …)`（现 `l_log_at_level`），删 `log_debug/info/warn/error` 四个一行转发（`Logger.cpp:180-183`）
- [x] prelude 定义**六级** `log_trace/log_debug/log_info/log_warn/log_error/log_fatal` 包装（trace/fatal 的脚本形态不因当前无调用点而缺席；C++ 侧 `log` 六级均可分派）
- [x] 改 `test_log_levels.cpp`（现按 6 个全局名逐个调）、`test_bind_global.cpp`（`l_log` 直测六级名）
- 调用点：62 处**零改动**
- 净减：−3 函数（C++ 侧 4 个一行转发删除、单一 `log` 入表；脚本侧 6 个名字由 prelude 齐备，调用点 0 改动。计划书原估 −5 含 B1 已删的 trace/fatal 两项，实际 C++ 口径为 −3）
- 运行记录：2026-11-11 check-binds 266/265/259（四方对账 OK）；`run-coverage` 2241+282 全过 delta +0.0%（Logger.cpp 65/70，`l_log` 各分支全覆盖）；`run-cppcheck` 无问题；`--check-lua`（bin\e2e exe）sample 70 / default 49 全 OK；dataset validate（已切 e2e exe）check-lua 5216 全 OK；集测 10 道+persist+reload 全 PASS（236s/420s，未登记 ERROR 0）

## B4 下沉② 等级换算 4→0 —— 工作量 S（调用点改动 0）

- [x] `skill2level` / `level2skill` / `Level2Exp` / `exp2level` 改由 prelude 实现：
  `Level2Exp` = `require("data.level_exp")[lv]`（数据源已在 `lua-default/data/level_exp.lua`，
  C++ `CharLevel.cpp:53-149` 只是缓存消费方，**C++ 内部函数不动**）
- [x] 删 4 个 `l_*`（`CharLevel.cpp:252-283`）+ decl + 测试用例（实际直测在 `test_bind_char_attr.cpp`，已就地改为前导 Lua 侧与 C++ 内部函数同式同源比对）
- [x] prelude 侧补 `lv>=176` 外推与 `require` 失败回退，与 C++ `generate_level_exp_approx` 行为对齐
- 调用点：41 处零改动
- 净减：−4 函数
- 运行记录：2026-11-11 check-binds 262/261/255（−4/−4/−4，四方对账 OK）；`run-coverage` 2241+282 全过 delta +0.0%（CharLevel.cpp 103/112）；`run-cppcheck` 无问题；`--check-lua`（e2e exe）sample 70/default 49 全 OK；dataset check-lua 5216 全 OK；集测 10 道+persist+reload 全 PASS（235s/420s，未登记 ERROR 0）；e2e 未命中清单 175→172（台账行归 B15 删）

## B5 下沉③ 称呼 4→1 + 谓词 4→1 —— 工作量 S（调用点改动 0）

- [x] 称呼：`query_respect` / `query_rude` / `query_self` / `query_self_rude` → `respect(kind, ob)`（C++ 合一落 `CharAttr.cpp`；四个旧名由 prelude 包装，调用点 0 改动）
- [x] 谓词：`is_user` / `is_wizard` / `is_living` / `is_character` → `is(ob, KIND)`（C++ 合一落跨类胶水 `LuaBindGlue.cpp`；三个全局旧名 + `index.is_character` 方法由 prelude 包装）
  ⚠️ **必须继续返回 boolean**：`Query(IS_LIVING)` 返回整数 0/1，Lua 中 0 为真，替换即成 `if 0 then` 事故
- [x] `is_char` 别名随本批并入 `is_character`/`is`（3 处改写：dataset `item/food/cake.lua`、`npc/d/lj/lj_tielao.lua` ×2）
- 调用点：292 + 1134 中**仅 3 处**改动
- 净减：−6 函数
- 运行记录：2026-11-11 check-binds 255/254/249（−7 条目/−7 名字/−6 函数，别名组 6→5，四方对账 OK）；`run-coverage` 2241+282 全过 delta +0.0%（`l_is`/`l_respect` 新增行全覆盖）；`run-cppcheck` 无问题；`--check-lua`（B5 e2e exe 12:50 构建）sample 70/default 49 全 OK；dataset check-lua 5216 全 OK；集测 10 道+persist+reload 全 PASS（231s/420s，未登记 ERROR 0）
- 附注：期间发现一次非本会话发起的 `run-integration-tests.ps1 -SkipBuild`（12:37，旧 B4 exe + 新 prelude → `global 'is'` 未注册、`setup_human` 全挂、10 道步数 0 全红）——属新旧版本错配的预期现象，已用完整重建的本批集测覆盖该结果；`cli/src/*.cc` 的逐步耗时 `[SLOW]` 诊断改动为并行工作线的未提交内容，本批未触碰

## B6 合并① 属性查询 7→1 —— 工作量 M（调用点 423 处）

- [x] `query_str` / `query_int` / `query_con` / `query_per` / `query_dex` / `query_combat_exp` / `query_exp` → 单个 `attr(name)`
- [x] 先删重复：`query_exp` 与 `query_combat_exp` 函数体逐字相同（`CharAttr.cpp:68,70`），23 处改指 `query_combat_exp`
      —— 实施时一步到位：两键合一，`query_exp` 的 23 处与 `query_combat_exp` 的 214 处
      共 237 处直改 `attr("combat_exp")`，无中间态
- [x] ⚠️ **性能裁定**：本族在 Lua 战斗公式里被调，**保留在 C++ 侧合并**，不走 prelude 下沉（下沉会把 1 次 C++ 调用换成 4~6 次 `query_skill` 调用）
- [x] **按用户指示把分派下沉到 C++**：`CChar::attr(const char* name, bool* ok = nullptr)` 是
      唯一分派点，`l_attr` 只做取对象 + 转发（4 行）。`ok` 出参让 Lua 侧能 `luaL_error`
      报错、C++ 侧常量键写错则 `LOG_ERROR`——键表只此一份，不会两侧漂移。
      C++ 侧 7 个 `CChar::query_*` 方法同批删除，45 处 C++ 调用点（13 文件）改 `attr("...")`
- 改动面：`Char.h`、`CharAttr.cpp`、`LuaBind.cpp`、`LuaBindDecls.h`、`test_bind_char_attr.cpp`、
      `test_char_attr.cpp`、`test_char_flush.cpp`、2 个 `.sce` 头注、src+tests 45 处 C++ 调用点、
      dataset/sample/default 423 处（170 文件）
- 净减：−6 函数
- 运行记录：2026-11-11 check-binds 249/248/243（−6 条目/−6 名字/−6 函数，四方对账 OK，
      重复 0、别名 5、PascalCase 41；0 调用仅剩 `prevent_learn`）；`run-coverage` 2243+285 全过
      delta +0.0%（**CharAttr.cpp 109/109 = 100%**，B6 前 145/154）；`run-cppcheck` 无问题；
      `--check-lua`（重建后的 `bin\e2e\jianghu.exe`，12:50 旧 exe 会误报 `attr` 未绑定）
      sample 70 / default 49 全 OK；dataset validate check-lua **5216 全 OK**（第 3 步
      roomcheck 74 未解析 = B1 记录的同数基线红，已核对与本批零交集：我的 423 行改动中
      携带房间引用者 0 行、命中未解析引用者 0 行，那 2 处 `add_npc`/`name =` 改动属既有
      未提交内容）；集测 10 道+persist pre/post+reload 全 PASS（227s/420s，关键路径 L3 226s，
      未登记 ERROR 0、白名单吸收 14）；e2e 未命中清单 172→169（台账行归 B15 删）
- 附注①：改名脚本一次性覆盖 423+45 处，落在 `test_tmp/rewrite_attr*.ps1`（gitignored，不入库）
- 附注②：改名脚本用 `UTF8Encoding($false)` 写回，曾把 15 个源文件 + 2 个 `.ps1` 的 BOM 剥掉；
      `.ps1` 掉 BOM 后 PS 5.1 按 GBK 解码、中文吃掉引号 → `validate.ps1` 6 处 / 
      `run-integration-tests.ps1` 94 处解析错误，已恢复。据此在 `AGENTS.md`「编码规范/风格」
      新增「文件编码（BOM）」节：非 `.ps1` 一律无 BOM、改到即改对；含非 ASCII 的 `.ps1`
      必须带 BOM。源码侧 BOM 已全部去掉（存量 57 个按「改到即改对」不动）
- 附注③：两个仓库的改动呈**已暂存**状态，但本会话全程只用 `status/diff/show` 只读命令、
      两个改名脚本也不含 `git add`；index 写入时间（dataset 17:47 / engine 17:51）晚于我
      最后一次文件编辑，非本会话所为，未作处置

## B7 合并② 装载/查找 11→3 —— 工作量 M（调用点 2059 + 295 处）

- [x] `load_room`(1016) / `load_item`(628) / `load_npc`(179) / `load_skill`(9) / `load_fb`(5) → `load(kind, name)`
- [x] `find_object`(102) / `find_online`(103) / `find_npc`(6) / `find_player`(2) / `find_item`(1) / `get_all_online_users`(8) → `find(kind, id)` + `find_online_users()`
- [x] `Get_ItemList`(295) / `get_item_list`(10) 同函数两名 → 只留 `get_item_list`（本批一并改）
- 改动面：`RoomMgr/ItemMgr/NpcMgr/SkillMgr/FB/UserMgr/Login/GameObject.cpp`、`test_bind_loader.cpp`、`test_bind_global.cpp`
- 净减：−8 函数（计划原估 −6；实际 11 个 `l_*` → `l_load`/`l_find`/`l_find_online_users` 三个，另 `Get_ItemList` 登记删除）
- 运行记录：2026-11-11 check-binds 240(175+65)/239/235（−9 条目/−9 名字/−8 函数，四方对账 OK、重复 0、别名 5→4、PascalCase 41→40、0 调用仍为 prevent_learn；`load` 1835 / `find` 216 / `find_online_users` 8 / `get_item_list` 305）；改写面 883 个 .lua 文件、2354 处调用点（`test_tmp/rewrite_b7.py` 按字节只改代码区，字符串/注释不碰），改写后旧名代码区残留 0；`run-coverage` 2245+285 全过 delta +0.0%（LuaBindGlue.cpp 263/278，`l_load`/`l_find` 52-113 行零未命中，各 KIND 支与未知 KIND 报错支全覆盖）；`run-cppcheck` 无问题；`--check-lua`（e2e exe 重建）sample 70 / default 49 全 OK；dataset check-lua 5216 全 OK；集测 10 道+persist+reload 全 PASS（229s/420s，未登记 ERROR 0，Δ +9s 未越 10s 参考值）；e2e 未命中清单 172→165（台账行归 B15 删）
- 附带修正①（既有测试隔离缺陷，分片重组后暴露）：`CharAttrTest` / `BindCharSkillTest` / `CharSaveTest` 在 Lua 环境外 `set_skill`，无脚本僵尸技能留在 `g_skill_list`，毒化后到的 `AttackDeepTest.ResolveDodgeBranchMatrix`（`dodge_msg` 恒空）——按 `test_combatcalc.cpp` 既有先例在三处 fixture 补 `ensure_lua_env()`
- 附带修正②：`c:\jianghu-dataset\_roomcheck.ps1` 的房间引用抽取正则随调用形态改为 `load("room", "…")`（否则改名后房间加载引用一条都抽不到，校验静默失效）；roomcheck 未解析 54→75 = 恢复基线红（B1 记录 74，dataset 内容基线）

## B8 合并③ 低使用族打包 —— 工作量 L（实际改写 1839 处，8 个子项可各自独立提交）

| 子项 | 现状 → 目标 | 调用点 | 净减 |
|---|---|---|---|
| [x] present 四式 | `present`(612) / `Present`(160) / `present_name`(41)+`PresentName`(72) → `present(kind, id)` | 885 | −2 |
| [x] 学徒招募 11→6 | `create_family`(556) 保留，其余 `assign_apprentice`(3)/`recruit_apprentice`(16)/`recognize_apprentice`(1)/`is_apprentice_of`(1)/`recruit_num`(1)/`release_master`(5) 收敛进 `apprentice(action,…)`；`query_recruit`/`update_recruit`/`learn_bonus` 保留；`prevent_learn` 归 B14 | 588 | −5 |
| [x] dbase map 9→2 | 按存储拆成 `dbase(action, key[, value])`（defined/map/map_keys/vec/get/set）+ `dbase_temp(action, key)`（map/map_keys/str），9 个旧名全收敛 | 406 | −7 |
| [x] 战斗结算 4→2 | `combat_attack`(234) / `combat_attack_unarmed`(4) / `combat_attack_weapon`(8) → `combat_attack(kind,…)`（normal/unarmed/weapon）；`combatd_query`(2) 保留 | 248 | −2 |
| [x] 金钱 2→1 | `pay_player`(36) / `player_pay`(12) → `pay_money(kind,…)`（give/take；裸名 `pay` 会撞 `house_base.lua` 全局变量，弃用） | 48 | −1 |
| [x] 描述三件 3→1 | `Long`(7) / `Short`(7) / `title`(2) → `desc(kind, [raw])` | 16 | −2 |
| [x] 死亡状态 4→3 | `die`(15)/`die_base`(69) → `die(kind: virtual\|base)`；**`unconcious`(50)/`revive`(35) 保留原名**（用户裁定：昏迷不是死，不入 `die`） | 169 | −1 |
| [x] 保存载入 4→2 | `Save`(58)+`save_to_local`(6) → `save_to_local`；`Load`(34)+`load_from_local`(3) → `load_from_local`；`LockData`(53)/`LoadFromServer`(10) 保留交 B9 | 164 | −2 |

- ⚠️ `Save`/`Load` 与 B9 命名归一重叠：谁先做谁改，另一批跳过
- 改动面：`ContainerItem/ContainerDbase/ContainerMisc/CharFamily/CharAttr/Combatd/Money/CharState/FightRoom.cpp` + 对应 `test_bind_*.cpp`
- 净减：−21 函数
- 运行记录：

## B9 命名归一 + 别名收敛 —— 工作量 L（调用点约 6070 处，需改写脚本）

- [ ] 42 个 PascalCase 名字 → `snake_case`（`naming.instructions.md` 对 Lua 侧的要求）：
  `SendMenu`(2684) / `AddMenuItem`(1089) / `UpdateMe`(544) / `Get_ItemList`(295) / `Present`(160) / `FlushMySkill`(135) / `Join_Team`(175) / `PresentName`(72) / `Add_Box`(94) / `Do_BiShi`(70) / `FlushMyInfo`(70) / `Begin_Fight`(64) / `Save`(58) / `SendState`(51) / `DownLoad`(17) / `Load`(34) / `Query`(104) / `MessageVision`(19) / `Level2Exp`(18) / `LockData`(53) / `PresentDoor`(22) / `End_Fight`(13) / `Locked`(12) / `Do_Wield`(9) / `Query_Amount`(27) / `Leave_Team`(20) / `Can_Join`(4) / `Query_Weight`(7) / `Split`(2) / `Long`(7) / `Short`(7) / `Do_UnWield`(6) / `MoveIn`(5) / `Begin_FB`(5) / `Select_Member`(3) / `RandomGetItem`(3) / `GetIP`(3) / `UnLocked`(3) / `DoActive`(3) / `Remove_Betrayer`(2) / `MakeCorpse`(5) / `LoadFromServer`(10)
  （其中 `Save`/`Load`/`Present`/`PresentName`/`Get_ItemList`/`Query` 若已在 B7/B8 改过则跳过）
- [ ] 顺带清 4 个 Pascal_Snake 混血：`Get_ItemList`、`Query_Amount`、`Query_Weight`、`Remove_Betrayer`
- [ ] 改写方式：一次性 AST/正则改写脚本（放 `test_tmp/`，不入库），逐仓 diff 抽查后提交
- 改动面：三仓约 6070 处 + `LuaBind.cpp` 两表 + decl + 16 个 `test_bind_*.cpp`
- 净减：−6 函数（6 组同函数多名收敛；纯改名不减数，减的是心智负担）
- 运行记录：

## B10 大族合并① 容器放置 5→1 —— 工作量 L（调用点 7419 处）

- [ ] `add_npc`(3098) / `add_door`(4054) / `add_item`(70) / `Add_Box`(94) / `add_object`(103) → `container:add(kind, opts)`
- [ ] 顺带把 `add_npc` 的 **8 个位置参数**（`Room.cpp:388-411`）改成表选项 `{name=, x=, y=, active=…}`
- 改动面：`Room.cpp`、`Box.cpp`、`test_bind_room.cpp`、三仓 7419 处
- 净减：−4 函数
- 运行记录：

## B11 大族合并② 技能映射/准备 9→3 —— 工作量 L（调用点 6841 处）

- [ ] `map_skill`(5859) / `prepare_skill`(852) / `query_skill_mapped`(103) / `query_skill_map_list`(8) / `query_skill_prepared`(7) / `query_skill_prepare_list`(6) / `clear_skill`(2) / `clear_skill_map`(2) / `clear_skill_prepare`(2) → `skill_map(action, …)` + `skill_prepare(action, …)` + `clear_skill(name)`
- 改动面：`CharSkill.cpp`、`test_bind_char_skill.cpp`、三仓 6841 处
- 净减：−6 函数
- 运行记录：

## B12 大族合并③ 消息 8→4 —— 工作量 L（调用点 10010 处）

- [ ] `message_vision`(2457) / `message_vision_self`(2) / `MessageVision`(19，即 `l_message_vision_channel`) / `message_channel`(12) → `message(kind, msg, …)` 或 `message_vision(msg, who, scope)`
- [ ] `tell_object`(3405) / `tell_room`(293) / `notify_fail`(3633) / `do_channel`(190) 评估保留（语义差异大，倾向不并）
- 改动面：`Message.cpp`、`Channel.cpp`、`test_bind_global.cpp`、三仓约 3500 处（若只并 vision 三式）
- 净减：−4 函数
- 运行记录：

## B13 大族合并④ temp 属性袋 4→2 —— 工作量 L（调用点 10111 处）

- [ ] `set_temp`(2965) / `query_temp`(3789) / `add_temp`(1667) / `delete_temp`(1690) → `set(k, v, "T")` / `query(k, "T")`（或保留 `set_temp` 名、只并 `add_temp`/`delete_temp` 进 `add`/`del` 的 temp 变体）
- ⚠️ 全计划最大改写量、且是热路径；**必须**与 B6 性能裁定同样先做基准对比再定形态
- 改动面：`ContainerDbase.cpp`、`test_bind_container_dbase.cpp`、三仓 10111 处
- 净减：−2 函数
- 运行记录：

## B14 胶水绑定甄别 —— 工作量 M（调用点 0）

- [ ] 7 个「Lua→C++ 方向天然无人走」的绑定按 `integration-test-adjudications.md §3` 口径逐个裁定：
  `l_prevent_learn`(0)、`l_create`、`l_die`、`l_do_channel`、`l_init`、`l_recognize_apprentice`、`l_room_reset`
  （同批的 `l_attempt_apprentice`、`l_do_use` 已移除）
- [ ] 判据：C++ 是否经虚函数/钩子直调脚本字段 —— 是则胶水方向可删，否则保留
- [ ] 删则连带 `test_bind_char_family.cpp:95` 等直接调用点
- 净减：−0 ~ −6 函数
- 运行记录：

## B15 文档同步与收口 —— 工作量 S

- [ ] 同步硬约束 §6 列出的四处文档（数字与台账行）
- [ ] `docs/e2e-unhit-audit.md §1` 中因合并/删除而消解的行就地删（按该文件规则：实施完成后该行即删）
- [ ] `check-binds.ps1` 并入收口命令清单
- [ ] 全量五命令 + 耗时报告三行硬指标核对
- 运行记录：

---

## 汇总

| 批次 | 工作量 | 调用点改动 | 净减函数 |
|---|---|---|---|
| B0 对账工具 | M | 0 | 0 |
| B1 删重+死码 | S | 0 | −2 |
| B2 prelude 机制 | M | 0 | 0 |
| B3 日志下沉 | S | 0 | −3（原估 −5 含 B1 已删的 trace/fatal） |
| B4 等级换算下沉 | S | 0 | −4 |
| B5 称呼+谓词 | S | 3 | −6 |
| B6 属性查询合并 | M | 423 | −6 |
| B7 装载/查找合并 | M | 2354 | −8 |
| B8 低使用族打包 | L | ≈2474 | −21 |
| B9 命名归一 | L | ≈6070 | −6 |
| B10 容器放置 | L | 7419 | −4 |
| B11 技能映射 | L | 6841 | −6 |
| B12 消息 | L | ≈3500 | −4 |
| B13 temp 属性袋 | L | 10111 | −2 |
| B14 胶水甄别 | M | 0 | −0~−6 |
| B15 文档收口 | S | 0 | 0 |
| **合计** | | **≈39,000** | **−72 ~ −78** |

**264 → 约 192（B14 全清约 186）。**
其中 **B0~B5 合计仅 3 处调用点改动，却已 −23 个绑定**，是性价比最高的一段。

> 实际进度（2026-11-11 收口 B0~B5）：264 → **249**（−15：B1 −2、B3 −3、B4 −4、B5 −6），
> 277 → 255 条目；三处调用点改动 = dataset 3 处 `is_char`→`is_character`。
> 上表 B3 行已按实际 −3 记；合计行的 −72 为全计划估算，随 B6~B15 实施逐批修正。

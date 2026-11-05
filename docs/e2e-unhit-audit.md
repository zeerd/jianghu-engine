# E2E 未命中函数审计（基线 2026-11-02）

> **性质**：阶段工具（查缺补漏清单），**不是设计文档**。批次收口后随条目一起删除；
> 不进 `docs/design/`，设计文档也不引用它。
> 依据：`copilot-instructions.md`「铁律：计划书与设计文档的分工」
> （计划书是任务清单、用完即删；查缺补漏清单是阶段工具，不是文档；逐函数细节不进设计文档正文）。
>
> **数据基线**
>
> | 项 | 来源 |
> |---|---|
> | E2E 覆盖率 | `coverage_report_e2e_sample\coverage.xml`（2026-11-05 01:41，pass，10 道 + persist 两段 + **reload 试验台**三者并行，exec 244s，`src` 行 **68.3%**（10624/15557），较 11-04 的 67%（10403/15534）Δ+1.3——增量来自热重载 E2E 打实的 reload 链与 sys 广播链） |
> | 单测覆盖率 | `coverage_report\coverage.xml`（2026-11-05 01:39，94.7%，delta +0.0） |
> | 官方清单 | `coverage_history\e2e-sample-unhit-functions.txt`（**251** 条 = 决策认领 220 + 待推进 31；11-04 时为 264，热重载链 13 条已随 reload 试验台转绿） |
> | 本审计结论 | 271 + 74 漏收 = **345** 条 |
>
> **2026-11-05 加密策略实施后复跑**（`run-integration-tests.ps1` 全绿，未登记 ERROR 0）：
> E2E `src` 行 **68.6%**（10831/15788）、单测 **94.7%**（delta +0.1）、官方清单 **249** 条（-2）。
> 本次改动未新增未命中函数——新口令/密钥函数全部落在登录、注册、开机建表三条链上；
> 未被 E2E 打实的只有派生失败等错误分支，由单测覆盖（见 `docs/design/encryption.md`）。
> 上表其余计数仍是 01:41 基线，未逐条重算。
>
> **方法与局限**（读数字前先看）
>
> - 函数体行区间用「从 `{` 做花括号配对」推得，签名行单独回溯，与清单口径一致。
> - 「调用方状态」按**函数名**匹配调用点，已剔除声明、绑定注册表、注释、字符串字面量。
>   但短名过泛（`create` / `reset` / `init` / `do_action` / `handle_action` / `query_attr`）
>   会跨类误配 → §5.4 的 63 条**必须逐条人工甄别**，不能直接当结论。
> - `unit_NODATA` = 单测覆盖率报告里**根本没有这些行**，原因见 §6.2，不等于「单测没测」。

---

## 1. 总账

- [ ] **确认总数：345 条 0 命中函数**（官方清单 271 + 本审计漏收 74）

### 1.1 按类型

| 类型 | 条数 | 清单内 | 漏收 |
|---|---|---|---|
| Lua 绑定 `l_*` | **140** | 128 | 12 |
| 巫师命令域 | **39** | 38 | 1 |
| 引擎侧 C++ | **166** | 105 | 61 |
| 合计 | **345** | 271 | 74 |

- [ ] 确认上表三行数字

### 1.2 按功能域

| 域 | 条数 |
|---|---|
| A Lua 绑定 `l_*` | 140 |
| B 巫师命令（`CCmdWizard::*` 等） | 39 |
| N Lua 运行时（Hook 结果读取 / 技能 Lua 化 / 热重载桥 / glue） | 34 |
| D 角色核心 `CChar::*` | 33 |
| H 房间 / 门 / 副本 | 17 |
| K 引擎（Channel / HeartBeat / GameObject） | 13 |
| M DB 层 | 12 |
| F 技能（`CSkill` / `SkillMgr`） | 12 |
| I NPC | 12 |
| J 容器 / dbase 绑定 | 9 |
| G 物品 / 装备 / 商店 | 8 |
| C 指令分发 / 社交 | 4 |
| E 战斗结算 | 4 |
| L 网络 / 登录 / 网关 | 3 |
| O user | 2 |
| P win32 托盘 UI | 2 |
| Q util（logger） | 1 |
| **合计** | **345** |

- [ ] 确认功能域分布表

### 1.3 单测交叉核对（见 §6）

| | 单测已命中 | 单测 0 命中 | 单测报告无此行 |
|---|---|---|---|
| Lua 绑定 | 135 | 5 | 0 |
| 巫师域 | 37 | 2 | 0 |
| 引擎侧 | 144 | 4 | 18 |
| **合计** | **316 (91.6%)** | **11** | **18** |

- [ ] 确认 **316/345 已被单测兜住**，E2E 的 0 命中是分工结果
- [ ] 确认**真正两边都没覆盖的只有 29 条**（11 + 18），清单见 §6.1

---

## 2. 清单生成器漏收 74 条（最高优先级）

`run-integration-tests.ps1` 的 `Get-SourceFunctions()` 找**独占一行的 `{`**、再往上回溯签名。
三个解析盲区导致 75 个 0 命中函数没进清单。**这是工具缺陷，不是测试缺口。**

- [ ] **盲区① 多行签名**：回溯只取 `{` 上一行，该行以 `)` 结尾但不含 `(` → `IndexOf('(') < 0` 被丢弃（**19 条，全为非绑定**，见 §2.1）
- [ ] **盲区② 签名与 `{` 之间有空行/注释**：`$sig.Count -eq 0` 直接 `continue`（**7 条，全为绑定**，见 §2.2）
- [ ] **盲区③ 单行函数体** `void f() {}`：要求 `{` 独占一行（**48 条 = 非绑定 43 + 绑定 5**，见 §2.2）
- [ ] 合计 20 + 7 + 48 = **75**；按类型分：绑定 12（7+5）、巫师 1（`print_svalue`）、引擎 62（19+43）
- [ ] **修 `Get-SourceFunctions()`**：①回溯跨空行继续找签名 ②允许 `)` 结尾行往上续 ③识别单行 `{...}` 函数体
- [ ] 修完重跑，核对清单从 **271 → 345**，且本文件 §2.1/§2.2 各条全部消失

### 2.1 盲区① 明细（19 条，全为非绑定）

- [ ] `CChar::receive_poison` — `src\char\CharCombat.cpp:281` 体 281-292 ｜ 盲区①
- [ ] `CChar::apply_condition` — `src\char\CharSkill.cpp:287` 体 287-303 ｜ 盲区①
- [ ] `CCmdWizard::print_svalue` — `src\cmd\CmdWizardWorld.cpp:117` 体 117-144 ｜ 盲区①（计入 §4 巫师域）
- [ ] `CCombatd::do_attack_weapon` — `src\combat\Combatd.cpp:29` 体 29-31 ｜ 盲区①
- [ ] `CReward::distribute_party_exp` — `src\combat\CReward.cpp:106` 体 106-159 ｜ 盲区①
- [ ] `CWeaponEffect::bash_weapon` — `src\combat\WeaponEffect.cpp:17` 体 17-51 ｜ 盲区①
- [x] ~~`db_update_recruit` — `src\db\SqlAccount.cpp:350` 体 350-391 ｜ 盲区①~~ **2026-11-04 20:19 起已命中**：玩家收徒场景（L6 存盘 → `CharSave` 师徒经验写链）
- [ ] `db_query_relative` — `src\db\SqlAccount.cpp:395` 体 395-422 ｜ 盲区①
- [ ] `db_save_object` — `src\db\SqlContainer.cpp:174` 体 174-232 ｜ 盲区①
- [ ] `db_load_object` — `src\db\SqlContainer.cpp:236` 体 236-264 ｜ 盲区①
- [ ] `db_load_room_boxes` — `src\db\SqlContainer.cpp:273` 体 273-299 ｜ 盲区①
- [ ] `create_database` — `src\db\SqliteDb.cpp:92` 体 92-98 ｜ 盲区①
- [ ] `sql_update_record` — `src\db\SqlRank.cpp:78` 体 78-139 ｜ 盲区①
- [ ] `CChannel::update_relative` — `src\engine\Channel.cpp:93` 体 93-95 ｜ 盲区①
- [x] ~~`CChannel::update_recruit` — `src\engine\Channel.cpp:104` 体 104-106 ｜ 盲区①~~ **2026-11-04 20:19 起已命中**：同上（写链入口）
- [x] ~~`CRelationshipTracker::update_recruit` — `src\engine\RelationshipTracker.cpp:47` 体 47-51 ｜ 盲区①~~ **2026-11-04 20:19 起已命中**：同上（写链入口）
- [ ] `CRelationshipTracker::query_relative` — `src\engine\RelationshipTracker.cpp:56` 体 56-69 ｜ 盲区①
- [ ] `CRelationshipTracker::update_relative` — `src\engine\RelationshipTracker.cpp:74` 体 74-78 ｜ 盲区①
- [ ] `CContainer::call_out_combat` — `src\object\ContainerMisc.cpp:44` 体 44-57 ｜ 盲区①
- [ ] `CSkill::valid_effect` — `src\skill\Skill.cpp:147` 体 147-149 ｜ 盲区①

### 2.2 盲区② 明细（7 条，全为绑定）+ 盲区③ 明细（48 条 = 非绑定 43 + 绑定 5）

> 本节共 55 个 checkbox：盲区② 的 7 条都是 `l_*` 绑定（同时计入 §3.1），
> 盲区③ 48 条中 43 条非绑定（本节列出）+ 5 条 `l_log_*` 绑定（同时计入 §3.1）。

**盲区②（签名与 `{` 之间有空行）—— 7 条**

- [ ] `l_db_execute` — `src\db\LuaBindDb.cpp:14` ｜ 计入 §3.1（**整个 `LuaBindDb.cpp` 36 行 0 命中，一条都没进清单**）
- [ ] `l_get_db_type` — `src\db\LuaBindDb.cpp:36` ｜ 计入 §3.1
- [ ] `l_db_register_ddl` — `src\db\LuaBindDb.cpp:47` ｜ 计入 §3.1
- [ ] `l_db_query` — `src\db\LuaBindDb.cpp:65` ｜ 计入 §3.1
- [ ] `l_call_out_combat` — `src\lua\LuaBindGlue.cpp:250` ｜ 计入 §3.1
- [ ] `l_handle_action` — `src\lua\LuaBindGlue.cpp:384` ｜ 计入 §3.1
- [ ] `l_base_create` — `src\lua\LuaBindGlue.cpp:415` ｜ 计入 §3.1

**盲区③（单行函数体，非绑定 43 条；另 5 条 `l_log_*` 见末尾）**

- [ ] `CChar::query_magic_exp` — `src\char\CharAttr.cpp:61`
- [ ] `CChar::query_grant` — `src\char\CharAttr.cpp:78`
- [ ] `CChar::attempt_apprentice` — `src\char\CharFamily.cpp:73`
- [ ] `CChar::send_off` — `src\char\CharFamily.cpp:87`
- [ ] `CChar::query_leader` — `src\char\CharFamily.cpp:171`
- [ ] `CChar::owner_is_killed` — `src\char\CharInterface.cpp:27`
- [ ] `CChar::wield` — `src\char\CharInterface.cpp:33`
- [ ] `CChar::wear` — `src\char\CharInterface.cpp:35`
- [ ] `CChar::do_action` — `src\char\CharInterface.cpp:41`
- [ ] `CChar::clear_skill_prepare` — `src\char\CharSkill.cpp:193`
- [ ] `CChar::clear_skill_map` — `src\char\CharSkill.cpp:195`
- [ ] `CChar::query_learned` — `src\char\CharSkill.cpp:241`
- [ ] `setup_monster` — `src\char\SetupChar.cpp:102`
- [ ] `CChannel::snoop` — `src\engine\Channel.cpp:74`
- [ ] `wiz_level` — `src\engine\GameObject.cpp:46`
- [ ] `CItem::do_use` — `src\item\Item.cpp:116`
- [ ] `lua_commands_flush` — `src\lua\LuaHook.cpp:150`
- [ ] `CGateInfo::x_send` — `src\net\GateInfo.cpp:102`
- [ ] `CNpc::chat_msg` — `src\npc\Npc.cpp:62`
- [ ] `CNpc::chat_msg_combat` — `src\npc\Npc.cpp:64`
- [ ] `CNpc::create` — `src\npc\Npc.cpp:86`
- [ ] `CNpc::greeting` — `src\npc\Npc.cpp:165`
- [ ] `CContainer::query_entire_temp_dbase` — `src\object\ContainerDbase.cpp:25`
- [ ] `CContainer::query_max_amount` — `src\object\ContainerWeight.cpp:63`
- [ ] `CContainer::set_amount` — `src\object\ContainerWeight.cpp:65`
- [ ] `CContainer::add_amount` — `src\object\ContainerWeight.cpp:67`
- [ ] `printf_all_mappings` — `src\object\Mapping.cpp:21`
- [ ] `CFB::create` — `src\room\FB.cpp:51`
- [ ] `CFB::init` — `src\room\FB.cpp:140`
- [ ] `CFB::reset` — `src\room\FB.cpp:155`
- [ ] `CRoom::create` — `src\room\Room.cpp:143`
- [ ] `CRoom::query_exits` — `src\room\Room.cpp:236`
- [ ] `CRoom::do_action` — `src\room\RoomInterface.cpp:21`
- [ ] `CRoom::handle_action` — `src\room\RoomInterface.cpp:23`
- [ ] `CSkill::has_magic` — `src\skill\Skill.cpp:47`
- [ ] `CSkill::query_action` — `src\skill\Skill.cpp:83`
- [ ] `CSkill::create` — `src\skill\Skill.cpp:125`
- [ ] `CSkill::reset` — `src\skill\Skill.cpp:127`
- [ ] `CSkill::query_dodge_msg` — `src\skill\Skill.cpp:153`
- [ ] `CSkill::practice_skill` — `src\skill\Skill.cpp:159`
- [ ] `CSkill::valid_force_pro` — `src\skill\Skill.cpp:173`
- [ ] `CSkill::hit_ob` — `src\skill\Skill.cpp:237`
- [ ] `l_log_trace` — `src\util\Logger.cpp:179` ｜ 计入 §3.1
- [ ] `l_log_debug` — `src\util\Logger.cpp:180` ｜ 计入 §3.1
- [ ] `l_log_warn` — `src\util\Logger.cpp:182` ｜ 计入 §3.1
- [ ] `l_log_error` — `src\util\Logger.cpp:183` ｜ 计入 §3.1
- [ ] `l_log_fatal` — `src\util\Logger.cpp:184` ｜ 计入 §3.1

---

## 3. Lua 绑定（140 条）

绑定可达链：`LuaBind.cpp` 注册表 → **脚本调用** → `l_xxx()`。
注册表本身不算调用，所以下面分两类。

### 3.1 教材从未调用：127 条（内容缺口，非测试缺口）

全量扫 `lua-default` + `lua-sample` 每个 `.lua`（排除注释行、排除 `function xxx.name(`
定义、区分 `:method()` 与全局函数）。**140 个绑定里只有 13 个有真实调用点，127 个一次都没出现。**

对应 `design/integration-test.md §6` 第 2 行裁定：**内容需求问题，不为测试加调用。**

- [ ] 确认口径：127 条属内容缺口，维持 `§6` 裁定不设消化目标
- [ ] `l_clear_condition` — `src\char\Char.cpp:373` `[clear_condition]`
- [ ] `l_reset_level` — `src\char\Char.cpp:397` `[reset_level]`
- [ ] `l_is_killing` — `src\char\Char.cpp:408` `[is_killing]`
- [ ] `l_disable_player` — `src\char\Char.cpp:455` `[disable_player]`
- [ ] `l_enable_player` — `src\char\Char.cpp:467` `[enable_player]`
- [ ] `l_query_dex` — `src\char\CharAttr.cpp:156` `[query_dex]` **【单测也未覆盖】**
- [ ] `l_query_combat_exp` — `src\char\CharAttr.cpp:167` `[query_combat_exp]`
- [ ] `l_query_apply` — `src\char\CharAttr.cpp:178` `[query_apply]`
- [ ] `l_query_str` — `src\char\CharAttr.cpp:190` `[query_str]`
- [ ] `l_query_con` — `src\char\CharAttr.cpp:201` `[query_con]`
- [ ] `l_query_int` — `src\char\CharAttr.cpp:212` `[query_int]`
- [ ] `l_query_per` — `src\char\CharAttr.cpp:223` `[query_per]`
- [ ] `l_is_invalid` — `src\char\CharAttr.cpp:234` `[is_invalid]`
- [ ] `l_learn_bonus` — `src\char\CharAttr.cpp:247` `[learn_bonus]`
- [ ] `l_query_respect` — `src\char\CharAttr.cpp:258` `[query_respect]`
- [ ] `l_query_rude` — `src\char\CharAttr.cpp:268` `[query_rude]`
- [ ] `l_query_self` — `src\char\CharAttr.cpp:278` `[query_self]`
- [ ] `l_query_self_rude` — `src\char\CharAttr.cpp:288` `[query_self_rude]`
- [ ] `l_query_valid_exp` — `src\char\CharAttr.cpp:307` `[query_valid_exp]`
- [ ] `l_wield_weapon` — `src\char\CharCombat.cpp:301` `[wield_weapon]`
- [ ] `l_unwield_weapon` — `src\char\CharCombat.cpp:321` `[unwield_weapon]`
- [ ] `l_base_attack` — `src\char\CharCombat.cpp:367` `[base_attack]` **【单测也未覆盖】**
- [ ] `l_receive_poison` — `src\char\CharCombat.cpp:410` `[receive_poison]`
- [ ] `l_set_killer` — `src\char\CharCombat.cpp:482` `[set_killer]`
- [ ] `l_fight_ob` — `src\char\CharCombat.cpp:495` `[fight_ob]`
- [ ] `l_remove_all_killer` — `src\char\CharCombat.cpp:508` `[remove_all_killer]`
- [ ] `l_set_leader` — `src\char\CharFamily.cpp:234` `[set_leader]`
- [ ] `l_recruit_num` — `src\char\CharFamily.cpp:247` `[recruit_num]`
- [ ] `l_release_master` — `src\char\CharFamily.cpp:256` `[release_master]`
- [ ] `l_is_apprentice_of` — `src\char\CharFamily.cpp:265` `[is_apprentice_of]`
- [ ] `l_recruit_apprentice` — `src\char\CharFamily.cpp:276` `[recruit_apprentice]`
- [ ] `l_assign_apprentice` — `src\char\CharFamily.cpp:304` `[assign_apprentice]`
- [ ] `l_recognize_apprentice` — `src\char\CharFamily.cpp:318` `[recognize_apprentice]`
- [ ] `l_prevent_learn` — `src\char\CharFamily.cpp:329` `[prevent_learn]`
- [ ] `l_attempt_apprentice` — `src\char\CharFamily.cpp:341` `[attempt_apprentice]`
- [ ] `l_query_recruit` — `src\char\CharFamily.cpp:353` `[query_recruit]`
- [ ] `l_update_recruit` — `src\char\CharFamily.cpp:384` `[update_recruit]`
- [ ] `l_query_left_weapon` — `src\char\CharFight.cpp:377` `[query_left_weapon]`
- [ ] `l_init` — `src\char\CharFight.cpp:395` `[init]`
- [ ] `l_do_bishi` — `src\char\CharFight.cpp:408` `[Do_BiShi]`
- [ ] `l_flush_my_skill` — `src\char\CharFlush.cpp:240` `[FlushMySkill]`
- [ ] `l_remove_betrayer` — `src\char\CharLevel.cpp:242` `[Remove_Betrayer]` **【单测也未覆盖】**
- [ ] `l_skill2level` — `src\char\CharLevel.cpp:253` `[skill2level]`
- [ ] `l_level2skill` — `src\char\CharLevel.cpp:261` `[level2skill]`
- [ ] `l_exp2level` — `src\char\CharLevel.cpp:277` `[exp2level]`
- [ ] `l_save_to_server` — `src\char\CharSave.cpp:322` `[SaveToServer]`
- [ ] `l_offline` — `src\char\CharSave.cpp:333` `[Offline]`
- [ ] `l_mini_skill` — `src\char\CharSkill.cpp:328` `[mini_skill]`
- [ ] `l_delete_skill` — `src\char\CharSkill.cpp:354` `[delete_skill]`
- [ ] `l_query_hardcore_skills` — `src\char\CharSkill.cpp:441` `[query_hardcore_skills]`
- [ ] `l_inc_skill` — `src\char\CharSkill.cpp:475` `[inc_skill]`
- [ ] `l_improve_skill` — `src\char\CharSkill.cpp:542` `[improve_skill]`
- [ ] `l_clear_skill` — `src\char\CharSkill.cpp:582` `[clear_skill]`
- [ ] `l_clear_skill_map` — `src\char\CharSkill.cpp:593` `[clear_skill_map]`
- [ ] `l_clear_skill_prepare` — `src\char\CharSkill.cpp:604` `[clear_skill_prepare]`
- [ ] `l_skill_need_points` — `src\char\CharSkill.cpp:615` `[skill_need_points]`
- [ ] `l_unconcious` — `src\char\CharState.cpp:296` `[unconcious]`
- [ ] `l_die` — `src\char\CharState.cpp:307` `[die]`
- [ ] `l_die_base` — `src\char\CharState.cpp:318` `[die_base]`
- [ ] `l_revive` — `src\char\CharState.cpp:342` `[revive]`
- [ ] `l_drop_all` — `src\cmd\CmdHandle.cpp:374` `[drop_all]`
- [ ] `l_get_room_char` — `src\cmd\CmdHandle.cpp:416` `[get_room_char]`
- [ ] `l_is_relative` — `src\cmd\CmdHandle.cpp:429` `[is_relative]`
- [ ] `l_damge_to_string` — `src\combat\Attack.cpp:663` `[damge_to_string]`
- [ ] `l_combat_attack_weapon` — `src\combat\Combatd.cpp:115` `[combat_attack_weapon]`
- [ ] `l_killer_reward` — `src\combat\CReward.cpp:499` `[killer_reward]`
- [ ] `l_db_execute` — `src\db\LuaBindDb.cpp:14` `[db_execute]` **【盲区②】**
- [ ] `l_get_db_type` — `src\db\LuaBindDb.cpp:36` `[get_db_type]` **【盲区②】**
- [ ] `l_db_register_ddl` — `src\db\LuaBindDb.cpp:47` `[db_register_ddl]` **【盲区②】**
- [ ] `l_db_query` — `src\db\LuaBindDb.cpp:65` `[db_query]` **【盲区②】**
- [ ] `l_sql_update_record` — `src\db\SqlRank.cpp:148` `[sql_update_record]`
- [ ] `l_make_random_yewai_global` — `src\engine\GameObject.cpp:172` `[make_random_yewai]`
- [ ] `l_random_get_item` — `src\engine\GameObject.cpp:201` `[RandomGetItem]`
- [ ] `l_make_corpse` — `src\item\Corpse.cpp:142` `[MakeCorpse]`
- [ ] `l_do_wield` — `src\item\Equip.cpp:221` `[Do_Wield]`
- [ ] `l_do_unwield` — `src\item\Equip.cpp:236` `[Do_UnWield]`
- [ ] `l_do_use` — `src\item\Item.cpp:280` `[do_use]`
- [ ] `l_do_action_base` — `src\item\Item.cpp:300` `[do_action_base]`
- [ ] `l_split` — `src\item\Item.cpp:337` `[Split]`
- [ ] `l_find_item` — `src\item\ItemMgr.cpp:140` `[find_item]`
- [ ] `l_random_vendor` — `src\item\Vendor.cpp:335` `[random_vendor]`
- [ ] `l_enter_register_hall` — `src\lua\LuaBindGlue.cpp:15` `[enter_register_hall]` **【单测也未覆盖】**
- [ ] `l_call_out_combat` — `src\lua\LuaBindGlue.cpp:250` `[call_out_combat]` **【盲区②】**
- [ ] `l_base_create` — `src\lua\LuaBindGlue.cpp:415` `[base_create]` **【盲区②】**
- [ ] `l_ini_get` — `src\lua\LuaBindGlue.cpp:671` `[ini_get]`
- [ ] `l_utf8_to_gbk` — `src\lua\LuaBindGlue.cpp:681` `[utf8_to_gbk]`
- [ ] `l_get_no_force_conflict` — `src\lua\LuaBindGlue.cpp:690` `[get_no_force_conflict]`
- [ ] `l_get_all_online_users` — `src\net\Login.cpp:563` `[get_all_online_users]`
- [ ] `l_message_vision_self` — `src\net\Message.cpp:440` `[message_vision_self]`
- [ ] `l_message_channel` — `src\net\Message.cpp:468` `[message_channel]`
- [ ] `l_chinese_number` — `src\net\Message.cpp:482` `[chinese_number]`
- [ ] `l_wield_ownered_item` — `src\npc\Npc.cpp:349` `[wield_ownered_item]`
- [ ] `l_random_move` — `src\npc\Npc.cpp:421` `[random_move]`
- [ ] `l_add_money` — `src\npc\Npc.cpp:430` `[add_money]`
- [ ] `l_random_npc` — `src\npc\NpcMgr.cpp:115` `[random_npc]`
- [ ] `l_create` — `src\object\Container.cpp:205` `[create]`
- [ ] `l_purge` — `src\object\Container.cpp:232` `[purge]`
- [ ] `l_clear` — `src\object\Container.cpp:241` `[clear]`
- [ ] `l_set_heart_beat` — `src\object\Container.cpp:252` `[set_heart_beat]`
- [ ] `l_get_map` — `src\object\ContainerDbase.cpp:194` `[get_map]`
- [ ] `l_set_map` — `src\object\ContainerDbase.cpp:223` `[set_map]`
- [ ] `l_queryvec` — `src\object\ContainerDbase.cpp:250` `[queryvec]`
- [ ] `l_querymap_temp_keys` — `src\object\ContainerDbase.cpp:343` `[querymap_temp_keys]`
- [ ] `l_title` — `src\object\ContainerItem.cpp:232` `[title]`
- [ ] `l_query_amount` — `src\object\ContainerWeight.cpp:122` `[Query_Amount]`
- [ ] `l_query_weight` — `src\object\ContainerWeight.cpp:133` `[Query_Weight]`
- [ ] `l_can_equipped` — `src\object\ContainerWeight.cpp:155` `[can_equipped]`
- [ ] `l_current_time_info` — `src\object\Global.cpp:49` `[current_time_info]`
- [ ] `l_get_server_id` — `src\object\Global.cpp:63` `[get_server_id]`
- [ ] `l_door_bind` — `src\room\Door.cpp:43` `[bind]`
- [ ] `l_draw_map` — `src\room\Room.cpp:369` `[draw_map]` **【单测也未覆盖】**
- [ ] `l_room_reset` — `src\room\Room.cpp:398` `[reset]`
- [ ] `l_remove_doors` — `src\room\Room.cpp:409` `[remove_doors]`
- [ ] `l_add_item` — `src\room\Room.cpp:478` `[add_item]`
- [ ] `l_present_door` — `src\room\Room.cpp:506` `[PresentDoor]`
- [ ] `l_room_exits` — `src\room\Room.cpp:521` `[room_exits]`
- [ ] `l_valid_force` — `src\skill\Skill.cpp:276` `[valid_force]`
- [ ] `l_load_skill` — `src\skill\SkillMgr.cpp:112` `[load_skill]`
- [ ] `l_do_active` — `src\user\User.cpp:453` `[DoActive]`
- [ ] `l_ready_pk` — `src\user\User.cpp:466` `[ready_pk]`
- [ ] `l_find_player` — `src\user\UserMgr.cpp:110` `[find_player]`
- [ ] `l_log_trace` — `src\util\Logger.cpp:179` `[log_trace]` **【盲区③】**
- [ ] `l_log_debug` — `src\util\Logger.cpp:180` `[log_debug]` **【盲区③】**
- [ ] `l_log_warn` — `src\util\Logger.cpp:182` `[log_warn]` **【盲区③】**
- [ ] `l_log_error` — `src\util\Logger.cpp:183` `[log_error]` **【盲区③】**
- [ ] `l_log_fatal` — `src\util\Logger.cpp:184` `[log_fatal]` **【盲区③】**
- [ ] `l_xrandom` — `src\util\Random.cpp:40` `[xrandom]`

### 3.2 教材调用了，但本轮走不到：12 条

- [ ] `l_query_team` — `src\char\Char.cpp:444` ← `throwing.lua:42`
  `me:is_fighting() and target:is_fighting() and me:query_team()==…` Lua 短路：偷袭发起时双方都没进战斗，`query_team()` 不求值
- [ ] `l_receive_wound` — `src\char\CharCombat.cpp:395` ← `force.lua:49`
  在 `if damage < 0` 内；`damage = mp//30 + factor - mp_you//100`，`mp_you` 封顶 `mp*3`（L40）→ `3/100 < 1/30≈3.33/100` → `damage ≥ factor ≥ 0`，**数学上不可能 <0**（`design §6` 已裁定）
- [ ] `l_is_fighting_target` — `src\char\CharFight.cpp:363` ← `force_adv.lua:97`
  在 `do_roar`（狮子吼）内，前置 `mp < 200 → return`；全仓无任何剧本 cast roar（`design §6` 已裁定 mp≥200 超经济封顶）
- [ ] `l_load_from_server` — `src\char\CharSave.cpp:311` ← `house_base.lua:131`、`resident_base.lua:138/149/179/221`
  全在 `if not owner then`（**房主离线**）分支；E2E 置业/卖房都由在线房主本人操作，`find_online()` 永远命中
- [ ] `l_add_user` — `src\user\UserMgr.cpp:118` ← 同上 5 处
  同上，离线房主重建分支不进
- [ ] `l_combatd_query` — `src\combat\Combatd.cpp:131` ← `house_base.lua:189`
  在 `house.reset()` 内；房间 reset 由 `HeartBeat::look_for_objects_to_swap` **每 900s** 触发，E2E 单轮 266s 不够
- [ ] `l_find_npc` — `src\npc\NpcMgr.cpp:123` ← `house_base.lua:196`
  同上，`house.reset()` 内
- [ ] `l_do_channel` — `src\engine\Channel.cpp:120` ← `house_base.lua:77/99`、`resident_base.lua:250`
  前两处是"注册区域非居住区"错误分支（`IS_RESIDENT` 恒 1 走不到）；第三处"长期欠费被收回"需跨天计费
- [ ] `l_find_object` — `src\engine\GameObject.cpp:240` ← `mirror_base.lua:173`
  在 `install` 装配的缺省 `die` 闭包里 `if npc.die == nil`，而 `sample_wooden_dummy` 自己定义了 `die`（L89）→ 缺省闭包从不装配
- [ ] `l_global_room_name` — `src\room\RoomMgr.cpp:81` ← `monster_yongren.lua:123/138`
  分支要求"房里有 fight_room 对象"或"房主 `balance < -Level2Exp(level)` 欠费被踢"，两条件本轮都不成立
- [ ] `l_room_count` — `src\room\RoomMgr.cpp:90` ← 同上 2 处
  同上
- [ ] `l_handle_action` — `src\lua\LuaBindGlue.cpp:384` ← `rk.lua:51` **【盲区②】**
  注册大厅 `register` 命令分支；`register.sce` 走 `/t 注册专员` + `/talk` 菜单，没走房间级 `register` 命令

---

## 4. 巫师命令域（39 条）

`design/integration-test.md §6` 第 1 行裁定：L9 验的是**服务协议本身**，
巫师命令只是工具面，服务链用到的 `full`/`set_skill`/`call` 已覆盖（副产品），
其余保持清单可见防盲区，**不设消化目标**。

- [ ] 确认维持 `§6` 巫师域裁定
- [ ] 确认证据：`tests/integration` 全仓搜 `wholist|promote|kickout|freeze|chpass|snoop|summon|smash|reboot` → **0 命中**；L9 只发协议命令

**单测也没覆盖的 2 条（需单独决策）**

- [ ] `CCmdWizard::reboot` — `src\cmd\CmdWizardSystem.cpp:473` **【单测也未覆盖】** → 重启服不敢在测试里跑，确认走「明确不测」还是补单测
- [ ] `CCmdWizard::do_reboot` — `src\cmd\CmdWizardSystem.cpp:480` **【单测也未覆盖】** → 同上（`call_out(do_reboot,1)` 回调）

**其余 37 条（单测已覆盖，E2E 不追）** — 明细 37 项：

- [ ] `CCmdWizard::promote` — `src\cmd\CmdWizardAdmin.cpp:5`
- [ ] `CCmdWizard::whois` — `src\cmd\CmdWizardAdmin.cpp:35`
- [ ] `CCmdWizard::kick` — `src\cmd\CmdWizardAdmin.cpp:63`
- [ ] `CCmdWizard::do_kick` — `src\cmd\CmdWizardAdmin.cpp:83`
- [ ] `CCmdWizard::ban` — `src\cmd\CmdWizardAdmin.cpp:100`
- [ ] `CCmdWizard::unban` — `src\cmd\CmdWizardAdmin.cpp:138`
- [ ] `CCmdWizard::freeze` — `src\cmd\CmdWizardAdmin.cpp:170`
- [ ] `CCmdWizard::unfreeze` — `src\cmd\CmdWizardAdmin.cpp:185`
- [ ] `CCmdWizard::chpass` — `src\cmd\CmdWizardAdmin.cpp:196`
- [ ] `CCmdWizard::chverifypass` — `src\cmd\CmdWizardAdmin.cpp:213`
- [ ] `CCmdWizard::who` — `src\cmd\CmdWizardAdmin.cpp:231`
- [ ] `CCmdWizard::recover` — `src\cmd\CmdWizardAdmin.cpp:291`
- [ ] `CCmdWizard::snoop` — `src\cmd\CmdWizardAdmin.cpp:314`
- [ ] `CCmdWizard::chblk` — `src\cmd\CmdWizardAdmin.cpp:361`
- [ ] `CCmdWizard::summon` — `src\cmd\CmdWizardSystem.cpp:28`
- [ ] `CCmdWizard::goto_cmd` — `src\cmd\CmdWizardSystem.cpp:54`
- [ ] `CCmdWizard::stop_battle` — `src\cmd\CmdWizardSystem.cpp:88`
- [ ] `log_level_name` — `src\cmd\CmdWizardSystem.cpp:107`
- [ ] `parse_log_level_name` — `src\cmd\CmdWizardSystem.cpp:114`
- [ ] `CCmdWizard::skill_param` — `src\cmd\CmdWizardSystem.cpp:210`
- [ ] `CCmdWizard::restore_exp` — `src\cmd\CmdWizardSystem.cpp:380`
- [ ] `CCmdWizard::clear_skill` — `src\cmd\CmdWizardSystem.cpp:402`
- [ ] `CCmdWizard::recruit_exp` — `src\cmd\CmdWizardSystem.cpp:428`
- [ ] `CCmdWizard::reward_exp` — `src\cmd\CmdWizardSystem.cpp:486`
- [ ] `CCmdWizard::sub_exp` — `src\cmd\CmdWizardSystem.cpp:511`
- [ ] `CCmdWizard::print_mapping` — `src\cmd\CmdWizardSystem.cpp:536`
- [ ] `CCmdWizard::save` — `src\cmd\CmdWizardSystem.cpp:586`
- [ ] `CCmdWizard::where_me` — `src\cmd\CmdWizardWorld.cpp:9`
- [ ] `CCmdWizard::clone_npc` — `src\cmd\CmdWizardWorld.cpp:33`
- [ ] `CCmdWizard::clone_object` — `src\cmd\CmdWizardWorld.cpp:49`
- [ ] `CCmdWizard::smash` — `src\cmd\CmdWizardWorld.cpp:84`
- [ ] `CCmdWizard::print_svalue` — `src\cmd\CmdWizardWorld.cpp:117` **【盲区①】**
- [ ] `CCmdWizard::show_data` — `src\cmd\CmdWizardWorld.cpp:148`
- [ ] `CCmdWizard::data` — `src\cmd\CmdWizardWorld.cpp:179`
- [ ] `set_invalid_account` — `src\net\LoginAuth.cpp:97`（只被 `freeze`/`unfreeze` 调）
- [ ] `db_set_freeze` — `src\db\SqlAccount.cpp:306`（同上）

**连带的 3 条非巫师函数**（因巫师域不追而断链）

- [ ] `CChar::skill_death_recover` — `src\char\CharLevel.cpp:165` ← 只被 `CCmdWizard::recover` 调
- [ ] `printf_all_mappings` — `src\object\Mapping.cpp:21` ← 只被 `CCmdWizard::print_mapping` 调
- [ ] `set_invalid_account`/`db_set_freeze` 已计入上表，不重复计数

---

## 5. 引擎侧 C++（166 条）

按「为什么 E2E 没跑到」分四档。**注意：按函数名匹配调用点，短名过泛会跨类误配。**

### 5.1 生产零调用方（死代码 / 仅单测调用）

**A. 真·零调用方 16 条** — 全仓 grep 确认**只有定义 + 头文件声明，或只有 `tests/unit` 调用**。

- [ ] `CChar::query_magic_skill` — `src\char\CharAttr.cpp:64` ｜ 仅 `test_char_attr.cpp` 调用
- [ ] `CChar::reset_level_exp_cache` — `src\char\CharLevel.cpp:63` ｜ 注释明写"供测试使用"，仅 `test_char_level.cpp`
- [ ] `CLuaSkill::get_magic_name` — `src\lua\LuaSkill.cpp:294` ｜ 仅 `test_force_adv_recover.cpp`
- [ ] `CLuaSkill::get_magic_display_name` — `src\lua\LuaSkill.cpp:305` ｜ 零调用（连单测也没有）
- [x] `lua_bind_check_object` — 已移出生产代码，读栈辅助归 `tests\unit\test_bind_common.h` ｜ 生产侧零调用方
- [ ] `logger_new_run_id` — `src\util\Logger.cpp:60` ｜ 仅 `test_env.cpp`/`test_log_runid.cpp`
- [ ] `CContainer::query_short` — `src\object\ContainerItem.cpp:60` ｜ 零调用 | 准备删除
- [ ] `CContainer::query_max_amount` — `src\object\ContainerWeight.cpp:63` ｜ 零 `->query_max_amount()` 调用点 | 需要重新设计。目前的堆叠功能没有上限。容易堆出一个超级重的物品。可能需要设置上限，大道上大道上达到上限自动分出第二堆。
- [ ] `CItem::query_max_amount` — `src\item\Item.cpp:124` ｜ 同上（虚函数无分派到）
- [ ] `CSkill::has_magic` — `src\skill\Skill.cpp:47` ｜ 零调用
- [ ] `CSkill::valid_effect` — `src\skill\Skill.cpp:147` ｜ 零调用 **【盲区①】**
- [ ] `CChar::query_leader` — `src\char\CharFamily.cpp:171` ｜ 零调用 **【盲区③】**
- [ ] `CChar::query_magic_exp` — `src\char\CharAttr.cpp:61` ｜ 零调用 **【盲区③】**

**B. 假调用点 3 条** — 初判为"链上"，复核后是字符串字面量造成的假调用点（`LOG_ERROR("db_save_object: …")` 被正则命中），实际零真实调用

- [ ] `db_save_object` — `src\db\SqlContainer.cpp:174` ｜ 被 `db_save_container` 取代，`Sql.h` 还留着声明
- [ ] `db_load_object` — `src\db\SqlContainer.cpp:236` ｜ 同上（被 `db_load_container` 取代）
- [ ] `db_load_room_boxes` — `src\db\SqlContainer.cpp:273` ｜ 同上

**C. 整链断头（入口零调用方 → 下游全断）** — 「玩家亲密度/仇敌关系」链从引擎入口起就没接线，`db_*` 层只有单测直接调

- [ ] `CChannel::update_relative` — `src\engine\Channel.cpp:93` ｜ 全仓无 `g_channel.update_relative(` 调用
- [ ] `CChannel::query_relative` — `src\engine\Channel.cpp:98` ｜ 同上
- [ ] `CRelationshipTracker::update_relative` — `src\engine\RelationshipTracker.cpp:74` ← 只被上者调
- [ ] `CRelationshipTracker::query_relative` — `src\engine\RelationshipTracker.cpp:56` ← 同上
- [ ] `db_update_relative` — `src\db\SqlAccount.cpp:425` ← 只被上者调
- [ ] `db_query_relative` — `src\db\SqlAccount.cpp:395` ← 同上
- [ ] 确认处置：**补接线 or 整链下线**（见 §8 A3）

> 注：`CChannel::update_recruit` / `CRelationshipTracker::update_recruit` / `db_update_recruit`
> 同属"上游命中但分支没进"，已归 §5.2；**2026-11-04 20:19 三条均已转正**（玩家收徒场景）。

### 5.2 调用方本身也未命中（链上断点）：48 条列于本节

> 口径：`ENGINE CHAIN` 共 **58** 条，其中 10 条因归属更清晰已分置他处——
> `CChar::skill_death_recover` 见 §4（巫师连带），
> `CChannel::query/update_relative` + `CRelationshipTracker::query/update_relative`
> + `db_query/update_relative` + `db_save/load_object` + `db_load_room_boxes` 共 9 条见 §5.1-C。
> 58 − 10 = **48**，即本节全部条目。

调用点全部落在别的 0 命中函数体内，一断全断。

- [ ] `CChannel::do_snooping` — `src\engine\Channel.cpp:82` ← `User.cpp`（`client/snoop` 需巫师 `snoop` 命令）
- [ ] `CChannel::query_snooping` — `src\engine\Channel.cpp:77` ← `CmdWizardAdmin.cpp`（巫师域）
- [x] ~~`CChannel::update_recruit` — `src\engine\Channel.cpp:104` ← `CharFamily.cpp` / `CharSave.cpp`（`family/user` 恒 0）~~ **2026-11-04 20:19 起已命中**：玩家收徒场景补上 `family/user=1` 侧
- [x] ~~`CRelationshipTracker::update_recruit` — `src\engine\RelationshipTracker.cpp:47` ← 同上~~ **2026-11-04 20:19 起已命中**
- [x] ~~`db_update_recruit` — `src\db\SqlAccount.cpp:350` ← 同上~~ **2026-11-04 20:19 起已命中**
- [ ] `CChar::clear_condition` — `src\char\Char.cpp:310` ← `l_clear_condition`（教材没调）
- [ ] `CChar::clear_skill` — `src\char\CharSkill.cpp:198` ← `l_clear_skill`（教材没调）
- [ ] `CChar::clear_skill_map` — `src\char\CharSkill.cpp:195` ← 同上
- [ ] `CChar::clear_skill_prepare` — `src\char\CharSkill.cpp:193` ← 同上
- [ ] `CChar::is_defeated` — `src\char\CharFight.cpp:157` ← `FightRoom.cpp`（演武场 wimpy 认输分支 0 命中）
- [ ] `is_defeated`（`CLuaNpcT`）— `src\lua\LuaNpc.inl:477` ← 同上
- [ ] `CChar::is_invalid` — `src\char\CharAttr.cpp:109` ← `l_is_invalid`（教材没调）
- [ ] `char_rule_limit` — `src\char\CharRules.cpp:12` ← `is_invalid`
- [ ] `CChar::query_apply` — `src\char\CharAttr.cpp:83` ← `l_query_apply`（教材没调）
- [ ] `CChar::query_grant` — `src\char\CharAttr.cpp:78` ← 同上 **【盲区③】**
- [ ] `CChar::mini_skill` — `src\char\CharSkill.cpp:261` ← `l_mini_skill`（教材没调）
- [ ] `CChar::query_learned` — `src\char\CharSkill.cpp:241` ← 同上 **【盲区③】**
- [ ] `CChar::receive_poison` — `src\char\CharCombat.cpp:281` ← `l_receive_poison`（教材没调）**【盲区①】**
- [ ] `CChar::recruit_num` — `src\char\CharFamily.cpp:206` ← `l_recruit_num`（教材没调）
- [ ] `CChar::remove_betrayer` — `src\char\CharLevel.cpp:206` ← `l_remove_betrayer`（教材没调）
- [ ] `CChar::skill2level` — `src\char\CharLevel.cpp:41` ← `l_skill2level`（教材没调）
- [ ] `CCmdHandle::do_kick` — `src\cmd\CmdHandle.cpp:351` ← `CmdWizardAdmin.cpp`（巫师域）
- [ ] `CCmdHandle::do_revive` — `src\cmd\CmdHandle.cpp:344` ← `CmdHandle.cpp`（`rate_limit=0` 关掉刷屏封禁分支）
- [ ] `CCombatd::do_attack_weapon` — `src\combat\Combatd.cpp:29` ← `l_combat_attack_weapon`（教材没调）**【盲区①】**
- [ ] `CContainer::call_out_combat` — `src\object\ContainerMisc.cpp:44` ← `l_call_out_combat`（教材没调）**【盲区①】**
- [ ] `CContainer::query_entire_temp_dbase` — `src\object\ContainerDbase.cpp:25` ← `l_querymap_temp_keys` 类链 **【盲区③】**
- [ ] `CGateInfo::x_send` — `src\net\GateInfo.cpp:102` ← `close_client` 改走 `close_connection()` 后已无生产调用方（死代码，归死代码批次处置）
- [ ] `CItem::destruct_me` — `src\item\Item.cpp:238` ← `CItem::receive_damage`（本身零调用）
- [ ] `CLuaSkill::has_query_action` — `src\lua\LuaSkill.cpp:228` ← `LuaSkill.cpp` 内部
- [ ] `CNpc::add_money` — `src\npc\Npc.cpp:135` ← `l_add_money`（教材没调）
- [ ] `CNpc::greeting` — `src\npc\Npc.cpp:165` ← `CLuaNpcT::greeting`（教材无 NPC 定义 `greeting`）**【盲区③】**
- [ ] `greeting`（`CLuaNpcT`）— `src\lua\LuaNpc.inl:582` ← 同上
- [ ] `CNpc::random_move` — `src\npc\Npc.cpp:256` ← `l_random_move`（教材没调）
- [ ] `CNpc::wield_ownered_item` — `src\npc\Npc.cpp:318` ← `l_wield_ownered_item`（教材没调）
- [ ] `CRoom::add_item` — `src\room\Room.cpp:239` ← `l_add_item`（教材没调）
- [ ] `CRoom::query_exits` — `src\room\Room.cpp:236` ← `l_room_exits`（教材没调）**【盲区③】**
- [ ] `CRoom::remove_doors` — `src\room\Room.cpp:274` ← `l_remove_doors`（教材没调）
- [ ] `CSkill::depends_on` — `src\skill\Skill.cpp:59` ← `SkillMgr.cpp`（热重载 `skill_release_one` 路径）
- [ ] `draw_map` — `src\room\RoomMgr.cpp:54` ← `l_draw_map`（教材没调）
- [ ] `printf_all_mappings` — `src\object\Mapping.cpp:21` ← `CCmdWizard::print_mapping`（巫师域）
- [ ] `random_get_item` — `src\engine\GameObject.cpp:20` ← `l_random_get_item`（教材没调）
- [ ] `random_npc` — `src\npc\NpcMgr.cpp:85` ← `l_random_npc`（教材没调）
- [ ] `random_vendor` — `src\item\Vendor.cpp:335` ← `l_random_vendor`（教材没调）
- [ ] `sql_update_record` — `src\db\SqlRank.cpp:78` ← `CFB::end_fb`（**`querymap("player")` 恒 NULL，全仓无人写 `player/` 键**）+ `l_sql_update_record`（教材没调）**【盲区①】**
- [ ] `lua_do_wield` — `src\lua\LuaItem.inl:513` ← `l_do_wield`（教材没调）
- [ ] `lua_do_un_wield` — `src\lua\LuaItem.inl:519` ← `l_do_unwield`（教材没调）
- [ ] `lua_do_action_base` — `src\lua\LuaItem.inl:526` ← `l_do_action_base`（教材没调）
- [ ] `tray_icon_restore` — `src\main.cpp:138` ← `WndProc`（托盘双击，需人工点）

### 5.3 调用方 0 命中（上游没跑到）：19 条

（原热重载链 10 条——三桥 ×3、`clear_loaded_shared_scripts`、notify ×3、
`skill_release_one`、`lua_commands_flush`、`dump_caller_info`——已随 2026-11-05
reload 试验台转绿移出）
- [ ] `CContainer::remove_call_out` — `src\object\ContainerMisc.cpp:61` ← `l_remove_call_out` 改走 `remove_call_out_by_id`
- [ ] `close_client` — `src\net\GateClient.cpp:13` ← `User.cpp:171`（`offline`）/ `ServerSockWorker.cpp:159`（登录失败），本轮两条都没走
- [ ] `send_to_client_direct` — `src\net\GateClient.cpp:47` ← `Login.cpp` 各错误分支（密码错/非 login 指令），本轮全部登录成功
- [ ] `set_charset` — `src\db\IDatabase.h:78` ← `Sql.cpp:185` **MySQL 分支**，E2E 走 SQLite
- [ ] `select_database` — `src\db\SqliteDb.cpp:101` ← `Sql.cpp:180` **MySQL 分支**
- [ ] `create_database` — `src\db\SqliteDb.cpp:92` ← `IDatabase` 纯虚，全仓无 `->create_database(` 调用 **【盲区①】**
- [ ] `generate_level_exp_approx` — `src\char\CharLevel.cpp:73` ← Lua `data.level_exp` 加载失败的兜底，本轮加载成功
- [ ] `setup_monster` — `src\char\SetupChar.cpp:102` ← `setup_char` 内分支 **【盲区③】**
- [ ] `wiz_level` — `src\engine\GameObject.cpp:46` ← `CmdStandardItem.cpp:32` **【盲区③】**
- [ ] `CLuaHookCall::result_bool` — `src\lua\LuaHook.cpp:404` ← `hooks.can_prepare_combine`（教材未实现该钩子）
- [ ] `CLuaHookCall::result_is_number` — `src\lua\LuaHook.cpp:410` ← `hooks.calculate_damage`（教材未实现）
- [ ] `CLuaHookCall::result_int` — `src\lua\LuaHook.cpp:416` ← 同上
- [ ] `CLuaHookCall::table_str_count` — `src\lua\LuaHook.cpp:473` ← `hooks.channel_group`（教材未实现）
- [ ] `CLuaHookCall::table_str_at` — `src\lua\LuaHook.cpp:480` ← 同上
- [ ] `CReward::victim_penalty` — `src\combat\CReward.cpp:46` ← `CharState.cpp:109`（受害者死亡惩罚，E2E 无合法触发面，`design §6` 已裁定）
- [ ] `CReward::distribute_party_exp` — `src\combat\CReward.cpp:106` ← **缺"组队状态下杀怪"场景**（见 §7）
- [ ] `CWeaponEffect::bash_weapon` — `src\combat\WeaponEffect.cpp:17` ← `Attack.cpp`，要求 `attack_type` ∈ {hammer, club, staff}，样本世界只有剑/拳/暗器 **【盲区①】**
- [ ] `CUser::user_dump` — `src\user\User.cpp:223` ← 需 `idle > IDLE_TIMEOUT(900s)`，单轮 266s 不够
- [ ] `tray_icon_menu` — `src\main.cpp:146` ← `WndProc WM_RBUTTONUP`，需人工点托盘

### 5.4 调用方已命中（分支没走到 / 同名噪声）：63 条

> ⚠ **本档含大量同名不同类误配**（`create`/`reset`/`init`/`do_action`/`handle_action`/
> `query_attr` 等短名），**不能直接当结论**。已人工核实干净的标 ✅，其余需逐条甄别。
> 另有 6 条是与 §5.1/§5.2 的交叉引用（不重复计数），本档实列 45 + 18 = **63**。

**已核实（干净）**

- [ ] ✅ `CChannel::snoop` — `src\engine\Channel.cpp:74` ← `User.cpp:63 if (query_temp("client/snoop"))` 行命中但体0 → 需先执行巫师 `snoop`
- [ ] ✅ `CChar::apply_condition`（4 参带 `who`）— `src\char\CharSkill.cpp:287` ← 教材 `apply_condition(cnd,duration)` 只传 3 参，`who` 恒 NULL → 走 3 参重载
- [ ] ✅ `CChar::handle_action`（4 参）— `src\char\CharInterface.cpp:46` ← 只有 `where`（环境）是角色才走，E2E 玩家永远在房间里
- [ ] ✅ `CRoom::download` / `CRoom::get_info` — `src\room\RoomInterface.cpp:70/80` ← 只有"房间对象被塞进容器"或 `downloadobject <房间id>` / `room:DownLoad(me)` 才走；`sample_master.lua:94 self:DownLoad(me)` 的 `self` 是 NPC → 走 `CChar::download`
- [ ] ~~✅ `CRelationshipTracker::update_recruit` 相关的 `CharSave.cpp:59` 分支 ← `family/user` 恒 0（E2E 收的都是 NPC 师父）~~ **2026-11-04 20:19 起已命中**：玩家收徒场景（L6 存盘触发 `if (query("family/user"))` 分支），NPC 师父侧另由 L4 的巫师 `call query family/user=0` 钉住
- [ ] ✅ `CFBRoom::CFBRoom(CFB*)` — `src\room\FBRoom.cpp:48` ← `create_fb_room` 的兜底分支（脚本缺失才走）
- [ ] ✅ `seed_first_id` — `src\db\DbTables.cpp:81` ← 只在**首建库**时调，E2E 库已存在
- [ ] ✅ `CChannel::group_listener` — `src\engine\ChatChannel.cpp:327` ← `hooks.channel_group` 教材未实现 → `group_key` 恒空
- [ ] ✅ `finish_kill`（`CLuaNpcT`）— `src\lua\LuaNpc.inl:446` ← `killer->finish_kill()`，需 **NPC 击杀玩家**，E2E 中 NPC 从未杀死玩家
- [ ] ~~✅ `CChar::load` — `src\char\CharSave.cpp:290` ← `LuaObjects.cpp` 恢复路径，E2E 用 `-ResetDb` 外的常驻库~~ **2026-11-04 起已命中**：persist 阶段黑风寨主 `create()` 的 `self:Load()` 走非用户分支（重启后 `restore_data` 恢复挑战计数即其实证）

**待甄别（同名噪声嫌疑，需逐条确认调用点是否真是本函数）**

- [ ] `can_combine_in_box` — `src\lua\LuaItem.inl:575` **【单测也未覆盖】**
- [ ] `CContainer::add_amount` — `src\object\ContainerWeight.cpp:67` **【单测也未覆盖】**
- [ ] `CItem::receive_damage` — `src\item\Item.cpp:196` **【单测也未覆盖】**（`CChar` 有同名重载，`CItem` 版本疑似零真实调用）
- [ ] `CItem::receive_wound` — `src\item\Item.cpp:215` **【单测也未覆盖】**（同上）
- [ ] `create_int_impl` — `src\lua\LuaRoom.inl:116` **【单测也未覆盖】**（模板标签分派 `false_type` 支）
- [ ] `desc_impl` — `src\lua\LuaRoom.inl:443` **【单测也未覆盖】**（同上）
- [ ] `valid_leave_impl` — `src\lua\LuaRoom.inl:375` **【单测也未覆盖】**（同上）
- [ ] `CChar::attempt_apprentice` — `src\char\CharFamily.cpp:73` **【盲区③】**
- [ ] `CChar::send_off` — `src\char\CharFamily.cpp:87` **【盲区③】**
- [ ] `CChar::owner_is_killed` — `src\char\CharInterface.cpp:27` **【盲区③】**
- [ ] `CChar::wield` / `CChar::wear` — `src\char\CharInterface.cpp:33/35` **【盲区③】**
- [ ] `CChar::do_action` — `src\char\CharInterface.cpp:41` **【盲区③】**
- [ ] `CChannel::update_relative` 已在 §5.1，不重复
- [ ] `CDoor::bind` — `src\room\Door.cpp:29`
- [ ] `CItem::do_use` — `src\item\Item.cpp:116` **【盲区③】**
- [ ] `CFB::change_room` — `src\room\FB.cpp:333`
- [ ] `CFB::do_look` — `src\room\FB.cpp:63`
- [ ] `CFB::create` / `CFB::init` / `CFB::reset` — `src\room\FB.cpp:51/140/155` **【盲区③】**
- [ ] `CRoom::create` — `src\room\Room.cpp:143` **【盲区③】**
- [ ] `CRoom::do_action` / `CRoom::handle_action` — `src\room\RoomInterface.cpp:21/23` **【盲区③】**
- [ ] `CSkill::create` / `CSkill::reset` / `CSkill::query_action` / `CSkill::query_dodge_msg` / `CSkill::practice_skill` / `CSkill::valid_force_pro` / `CSkill::hit_ob` — `src\skill\Skill.cpp:125/127/83/153/159/173/237` **【盲区③】**
- [ ] `CSkill::query_attr` — `src\skill\Skill.cpp:86`（`default:` 支；`IS_SKILL` case 也 0）
- [ ] `CNpc::create` / `CNpc::chat_msg` / `CNpc::chat_msg_combat` — `src\npc\Npc.cpp:86/62/64` **【盲区③】**
- [ ] `CNpc::do_talk` — `src\npc\Npc.cpp:193`
- [ ] `CNpc::set_inquiry` ×2 — `src\npc\Npc.cpp:146/156`
- [ ] `CNpc::return_home` — `src\npc\Npc.cpp:68`
- [ ] `CChar::wield_position` — `src\char\Char.cpp:242`
- [ ] ~~`CChar::load` — `src\char\CharSave.cpp:290`~~ **2026-11-04 起已命中**（persist 阶段 `self:Load()`，见 §5.4 同名噪声节）
- [ ] `CChar::is_defeated` 已在 §5.2
- [ ] `CCombatd::do_attack_weapon` 已在 §5.2
- [ ] `CContainer::set_amount` — `src\object\ContainerWeight.cpp:65` **【盲区③】**
- [ ] `CContainer::query_entire_temp_dbase` 已在 §5.2
- [ ] `CVector::get_buffer` — `src\object\Mapping.cpp:538`（存盘实际走 `CMapping::get_buffer`）
- [ ] `CChatChannel::group_listener` 已标 ✅
- [ ] `CItem::destruct_me` 已在 §5.2
- [ ] `IDatabase::create` — `src\db\SqliteDb.cpp:135`（实际用 `create_sqlite_db`/`create_my_sql_db` 自由函数）
- [ ] `handle_action` ×3（`LuaRoom.inl` / `LuaNpc.inl` / `LuaItem.inl`）— 短名噪声，需按签名区分 3 参/4 参
- [ ] `CCmdUser::mv` — `src\cmd\CmdUserSocial.cpp:453`（`design §6` 低价值透传）
- [ ] `CCmdUser::nullify_vote` — `src\cmd\CmdUserSocial.cpp:297`（`call_out 600s`，超单轮；`design §6` 已裁定）

**待甄别（续，同名噪声嫌疑）**

- [ ] `CEquip::can_equip` — `src\item\Equip.cpp:22`
- [ ] `CVendor::do_action` — `src\item\Vendor.cpp:36`
- [ ] `CLuaSkill::base_query_action` — `src\lua\LuaSkill.cpp:215`
- [ ] `CLuaSkill::valid_force_pro` — `src\lua\LuaSkill.cpp:471`
- [ ] `CFBRoom::CFBRoom` — `src\room\FBRoom.cpp:48`
- [ ] `CRoom::get_info` — `src\room\RoomInterface.cpp:80`（见 ✅ 项 `CRoom::download`，同链）
- [ ] `CRoom::handle_action` — `src\room\RoomInterface.cpp:23` **【盲区③】**
- [ ] `CChar::wear` — `src\char\CharInterface.cpp:35` **【盲区③】**
- [ ] `CNpc::chat_msg` — `src\npc\Npc.cpp:62` **【盲区③】**
- [ ] `CNpc::chat_msg_combat` — `src\npc\Npc.cpp:64` **【盲区③】**
- [ ] `CFB::init` — `src\room\FB.cpp:140` **【盲区③】**
- [ ] `CFB::reset` — `src\room\FB.cpp:155` **【盲区③】**
- [ ] `CSkill::query_action` — `src\skill\Skill.cpp:83` **【盲区③】**（`CLuaSkill::query_action` 已命中，基类版疑似零分派）
- [ ] `CSkill::reset` — `src\skill\Skill.cpp:127` **【盲区③】**
- [ ] `CSkill::query_dodge_msg` — `src\skill\Skill.cpp:153` **【盲区③】**
- [ ] `CSkill::practice_skill` — `src\skill\Skill.cpp:159` **【盲区③】**
- [ ] `CSkill::valid_force_pro` — `src\skill\Skill.cpp:173` **【盲区③】**
- [ ] `CSkill::hit_ob` — `src\skill\Skill.cpp:237` **【盲区③】**

---

## 6. 单测也没覆盖的 28 条（真正的双盲缺口）

### 6.1 单测报告里有行但 0 命中（11 条）

- [ ] `l_base_attack` — `src\char\CharCombat.cpp:367`
- [ ] `l_draw_map` — `src\room\Room.cpp:369`
- [ ] `l_enter_register_hall` — `src\lua\LuaBindGlue.cpp:15`
- [ ] `l_query_dex` — `src\char\CharAttr.cpp:156`
- [ ] `l_remove_betrayer` — `src\char\CharLevel.cpp:242`
- [ ] `can_combine_in_box` — `src\lua\LuaItem.inl:575`（`m_script_ref == LUA_NOREF` 兜底支）
- [ ] `CContainer::add_amount` — `src\object\ContainerWeight.cpp:67`（空实现，调用行 0）
- [ ] `CChar::user_dump` — `src\user\User.cpp:223`（需 idle > 900s）
- [ ] `CCmdWizard::reboot` — `src\cmd\CmdWizardSystem.cpp:473`
- [ ] `CCmdWizard::do_reboot` — `src\cmd\CmdWizardSystem.cpp:480`
- [ ] `CCmdHandle::do_revive` — `src\cmd\CmdHandle.cpp:344`（`rate_limit=0` 关掉刷屏封禁）

### 6.2 单测报告里根本没有这些行（17 条）

`unit_NODATA` = 单测覆盖率报告里**完全没有这些行**。按成因分三组：

**① `run-coverage.ps1` 的 `$excludedFiles` 排除 `main.cpp`（单测有自己的 main）— 2 条**

- [x] `tray_icon_restore` — `src\main.cpp:138` - 忽略
- [x] `tray_icon_menu` — `src\main.cpp:146` - 忽略

**② `tests.vcxproj` 开了 `/OPT:REF`+`/OPT:ICF`（L107-108），未引用 COMDAT 被链接器剔除；`jianghu.vcxproj` 没开 → 反证生产里也没有引用者 — 12 条**

- [ ] `CChannel::query_relative` — `src\engine\Channel.cpp:98`
- [ ] `CChannel::update_relative` — `src\engine\Channel.cpp:93`
- [ ] `CRelationshipTracker::query_relative` — `src\engine\RelationshipTracker.cpp:56`
- [ ] `CRelationshipTracker::update_relative` — `src\engine\RelationshipTracker.cpp:74`
- [ ] `CCmdHandle::do_kick` — `src\cmd\CmdHandle.cpp:351`
- [ ] `CItem::receive_damage` — `src\item\Item.cpp:196`
- [ ] `CItem::receive_wound` — `src\item\Item.cpp:215`
- [ ] `CItem::destruct_me` — `src\item\Item.cpp:238`
- [ ] `CLuaSkill::get_magic_display_name` — `src\lua\LuaSkill.cpp:305`
- [ ] `setup_monster` — `src\char\SetupChar.cpp:102`

**③ 模板实例化集合差异（`CLuaRoomT` 的 `false_type` 标签分派在单测二进制里没被实例化）— 3 条**

- [ ] `create_int_impl`（`false_type` 支）— `src\lua\LuaRoom.inl:116`
- [ ] `valid_leave_impl`（`false_type` 支）— `src\lua\LuaRoom.inl:375`
- [ ] `desc_impl`（`false_type` 支）— `src\lua\LuaRoom.inl:443`

（原 ④ 其余 1 条 `reload_script`（`LuaItem.inl:39`）已随 2026-11-05 reload 试验台
在 E2E 侧转绿，不再是双盲缺口）

- [ ] 逐条确认上表 17 条归因（尤其 ② 组 12 条 → 归 §5.1 死代码）

---

## 7. 补一条剧本就能拿到的（最高性价比）

- [ ] **组队状态下杀怪**：`party_a.sce` 只验组队命令不杀怪，7 个含 `kill` 的剧本（`sneak_and_battle`/`servant`/`fight_a`/`pk_a`/`raid_assault`/`prepare_combine`）都不组队 → `CReward::distribute_party_exp`（经验共享）整块 0 覆盖。**这是唯一一条"补场景即得"的高价值缺口。**
- [ ] **`house_base.lua` 的 `player/` 映射全仓无人写** → `CFB::end_fb` 排行写入分支（`sql_update_record`）永远走不到；`rank.sce` 覆盖的只是查询侧。确认是补内容还是标废弃。
- [x] ~~`poison_base.lua` 的涂毒成功支与 `l_send_state` 到不了~~ **已注销**：E2E（`poison_and_cure.sce` 的涂毒链）打实 `do_look` → `do_mo` 成功支挂 `poison/modu` → 二涂被拒 → `remove_effect` 到点解除，`l_send_state` 随之进覆盖（未命中清单已无此项）；模块由 `sample_shedan.lua` `require`，不再是孤儿。单测侧另有 `SampleFoodPoisonTest` 的 `Shedan_TumoWithPoisonSkillCoatsWeapon` / `PoisonBase_HandleActionRoutesUseTumoAndUnknown` 兜底（毒术等三前置的分支矩阵）。
- [x] ~~**`family/user` 恒为 0**：E2E 收的都是 NPC 师父，`recruit_apprentice` 只在 `is_user(master)` 时写 `family/user=1` → 师徒经验**写**链（`update_recruit`→`db_update_recruit`）不可达。确认是否补"玩家收徒"场景。~~ **已注销（2026-11-04）**：补上玩家收徒场景——L5/L6 师徒四段（`interaction/recruit_*`/`rexp_*`/`expell_*`/`apprentice_*`，A 扮师父、B 扮徒弟）+ L4 的 `social/npc_master_family_user.sce`（NPC 师父 `family/user=0` 反向锚点）。写链（存盘→`update_recruit`→`db_update_recruit`）与 `release_master` 翻标记两侧都有断言，未命中清单已无 `update_recruit`/`db_update_recruit`/`release_master`/`CCmdStandard::recruit|expell`/`CCmdUser::recruit_exp`
- [ ] **`CFB` 的 `querymap("player")` 恒 NULL**（见上）
- [ ] **`mirror_base` 缺省 `die` 从不装配**：`sample_wooden_dummy` 自定义了 `die` → `find_object` 不可达。确认是否需要一个不自定义 `die` 的镜像 NPC。

---

## 8. 建议动作

- [ ] **A1 修 `Get-SourceFunctions()` 三个盲区**（§2），清单 271 → 345。成本最低、收益最大。
- [x] **A2 删除死代码**：`db_save_object`/`db_load_object`/`db_load_room_boxes`（旧 API，已被 `*_container` 取代，`Sql.h` 还留声明）
- [ ] **A3 处理 `CChannel::update/query_relative` 断头**：`RelationshipTracker` 建了表、`db_*` 有单测，但引擎入口零调用方 → 要么补接线，要么整链下线
- [ ] **A4 单测补 11 条 `unit_MISS`**（§6.1），其中 5 条绑定、2 条巫师 reboot、1 条 `user_dump`（单测里直接调即可）
- [ ] **A5 §5.4 待甄别条目逐条过一遍**，把同名噪声剔掉后重新归档
- [ ] **A6 `design §6` 裁定复核**：巫师域、`victim_penalty`、`mv`/`nullify_vote`、内力经济封顶（`receive_wound`/`is_fighting_target`）——确认维持
- [ ] **A7 本文件**：批次收口后随条目一起删除，不进 `docs/design/`，设计文档不引用

---

## 9. 复核用数据文件

- [x] 确认以下中间产物已归档或随本文件一起清理：
  - `coverage_history\e2e-unhit-claimed.txt`（既有认领登记，保持不动）
  - 本审计中间产物（原在 `%TEMP%`，已随「测试临时文件一律入仓库」清理删除）：
    `blind2.csv`、`blind3.csv`、`bind_never_called.csv`、`final_matrix_e2e.csv`、`final_domains.csv`

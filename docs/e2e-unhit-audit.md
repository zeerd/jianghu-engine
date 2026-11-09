# E2E 未命中函数清单（按功能分组）

> **数据**：2026-11-09 18:19 轮 E2E（pass，src 行 71.6%），原始 281 条 = 官方清单 201 + 解析漏收 80。
> **口径与局限**见 `docs/design/integration-test.md` §5.3；**域级不测裁定**见其 §6。
>
> **本清单只留未决条目**（现 163 条）：
> - 已完成处置（教材落地 / 死码删除 / 转命中）的条目随收口直接删除；
> - 已裁定「明确不测」的条目归档到三处后删除：**结论** = design §6、**证据** =
>   `docs/plans/integration-test-adjudications.md`、**清单原行** =
>   `coverage_history/e2e-unhit-claimed.txt`（每轮未命中清单按原行分成「决策认领 / 待推进」）；
> - 处置方案写在各行 `| 质疑 → 方案` 后，**实施完成后该行即删**。
>
> 「移入 §6 / 全仓无调用方」类断言须逐条拿 `c:\jianghu-dataset` 复核：dataset 有真实调用的
> 不删、重新规划；dataset 也无调用的才按死码处置。

## 1. Lua 绑定 l_*（脚本侧可调的引擎 API）（73 条）

- [ ] `l_reset_level` — src\char\Char.cpp:397
- [ ] `l_disable_player` — src\char\Char.cpp:455
- [ ] `l_enable_player` — src\char\Char.cpp:467
- [ ] `l_is_invalid` — src\char\CharAttr.cpp:214
- [ ] `l_learn_bonus` — src\char\CharAttr.cpp:227
- [ ] `l_query_respect` — src\char\CharAttr.cpp:238
- [ ] `l_query_rude` — src\char\CharAttr.cpp:248
- [ ] `l_query_self` — src\char\CharAttr.cpp:258
- [ ] `l_query_self_rude` — src\char\CharAttr.cpp:268
- [ ] `l_receive_wound` — src\char\CharCombat.cpp:362 | 质疑 → 方案：（参考random_npc）主动申请取消任务，发布者调用这个给玩家惩罚。
- [ ] `l_set_leader` — src\char\CharFamily.cpp:234
- [ ] `l_recruit_num` — src\char\CharFamily.cpp:247
- [ ] `l_is_apprentice_of` — src\char\CharFamily.cpp:265
- [ ] `l_recruit_apprentice` — src\char\CharFamily.cpp:276
- [ ] `l_assign_apprentice` — src\char\CharFamily.cpp:304
- [ ] `l_query_recruit` — src\char\CharFamily.cpp:342
- [ ] `l_update_recruit` — src\char\CharFamily.cpp:373
- [ ] `l_init` — src\char\CharFight.cpp:383 | 质疑：需要调查dataset的使用情况 → 方案：已调查——dataset 11 处在用（密室房间 add_npc 后 `:init(me)`，cell.h 同款惯例），非死码；**sample 载体方案已撤回**（原拟给黑风寨刷出的大当家补 `boss:init(me)`，与 L3「路过大厅不打山贼」及 persist 通行的共享单例设计冲突）——需另择载体（候选：新增密室式「刷出即迎敌」事件，C 类）
- [ ] `l_skill2level` — src\char\CharLevel.cpp:253
- [ ] `l_level2skill` — src\char\CharLevel.cpp:261
- [ ] `l_exp2level` — src\char\CharLevel.cpp:277
- [ ] `l_mini_skill` — src\char\CharSkill.cpp:328
- [ ] `l_inc_skill` — src\char\CharSkill.cpp:440
- [ ] `l_clear_skill` — src\char\CharSkill.cpp:547
- [ ] `l_clear_skill_map` — src\char\CharSkill.cpp:558
- [ ] `l_clear_skill_prepare` — src\char\CharSkill.cpp:569
- [ ] `l_skill_need_points` — src\char\CharSkill.cpp:580
- [ ] `l_get_room_char` — src\cmd\CmdHandle.cpp:398
- [ ] `l_is_relative` — src\cmd\CmdHandle.cpp:411
- [ ] `l_combatd_query` — src\combat\Combatd.cpp:131
- [ ] `l_random_get_item` — src\engine\GameObject.cpp:172
- [ ] `l_do_wield` — src\item\Equip.cpp:221 | 质疑：与 l_wield的关系？从实际功能层面有啥么差异？为什么封装了两个？ → 方案：已查明——`wield` 是动作入口（走 do_action 分发、可被脚本截获），`Do_Wield` 是装备实现体（供覆写 do_action 的物品回落调用，否则在 do_action 里调 wield 会递归回自身）；两问即教材：sample 补一件覆写 do_action 的物品（参考 dataset gaitou/killermask 模式）回落 Do_Wield，与 do_action_base 同场景带出
- [ ] `l_do_unwield` — src\item\Equip.cpp:236 | 质疑：同 l_do_wield → 方案：同上——`Do_UnWield` 是卸下实现体，随同一件覆写 do_action 的教材物品一并带出
- [ ] `l_do_action_base` — src\item\Item.cpp:281 | 质疑 → 方案：同 do_wield 一族——dataset 的覆写 do_action 物品普遍 `return self:do_action_base(nAct, me)` 回落，sample 同款教材物品一并带出
- [ ] `l_split` — src\item\Item.cpp:318
- [ ] `l_find_item` — src\item\ItemMgr.cpp:140
- [ ] `l_random_vendor` — src\item\Vendor.cpp:335
- [ ] `l_enter_register_hall` — src\lua\LuaBindGlue.cpp:15
- [ ] `l_ini_get` — src\lua\LuaBindGlue.cpp:675
- [ ] `l_get_all_online_users` — src\net\Login.cpp:563
- [ ] `l_message_channel` — src\net\Message.cpp:468
- [ ] `l_random_npc` — src\npc\NpcMgr.cpp:115 | 质疑：添加一个任务系统：要求把金创药交给随机的NPC。任务新系统做个单独的quest_base.lua/accept_object，让所有NPC（非FB）去require。 → 方案：新增 `lua-sample/data/quest_base.lua`（accept_object 目标=把金创药交给 `random_npc()` 指定的 NPC），非 FB NPC 的 create 统一 require，确定商人为任务发布者，配一段接任务→交药的剧本（dataset `quest_mgr.lua` 已有同款 `random_npc()` 用法可作参照）
- [ ] `l_find_npc` — src\npc\NpcMgr.cpp:123
- [ ] `l_purge` — src\object\Container.cpp:232
- [ ] `l_clear` — src\object\Container.cpp:241
- [ ] `l_set_heart_beat` — src\object\Container.cpp:252
- [ ] `l_get_map` — src\object\ContainerDbase.cpp:194
- [ ] `l_set_map` — src\object\ContainerDbase.cpp:223
- [ ] `l_queryvec` — src\object\ContainerDbase.cpp:250
- [ ] `l_querymap_temp_keys` — src\object\ContainerDbase.cpp:343
- [ ] `l_title` — src\object\ContainerItem.cpp:232
- [ ] `l_query_amount` — src\object\ContainerWeight.cpp:122
- [ ] `l_query_weight` — src\object\ContainerWeight.cpp:133
- [ ] `l_can_equipped` — src\object\ContainerWeight.cpp:155
- [ ] `l_current_time_info` — src\object\Global.cpp:49
- [ ] `l_get_server_id` — src\object\Global.cpp:63
- [ ] `l_door_bind` — src\room\Door.cpp:43 | 质疑 → 方案：绑定在册但教材无「改门向/动态门」场景，需补教材：一段 talk 改门向的动态门片段
- [ ] `l_add_item` — src\room\Room.cpp:446 | 质疑 → 方案：教材已在用（`sample_guide.lua:75` 刷怪树，treebox.sce 在 `wait object` 等它），未命中新轮疑为上轮残留怪树使 `random/tree=1` 跳过首触——复核该轮状态，必要时测试侧复位标记，不改代码
- [ ] `l_present_door` — src\room\Room.cpp:474
- [ ] `l_room_exits` — src\room\Room.cpp:489
- [ ] `l_global_room_name` — src\room\RoomMgr.cpp:65
- [ ] `l_room_count` — src\room\RoomMgr.cpp:74
- [ ] `l_valid_force` — src\skill\Skill.cpp:276 | 质疑 → 方案：dataset `hooks.char_rule` 的 cast_force 分支在用，不可删；E2E 命中要补 cast_force 分支——**必须落 lua-sample/data/hooks.lua**（E2E 不加载 lua-default 的 hooks，已调研），随教材批次落地
- [ ] `l_load_skill` — src\skill\SkillMgr.cpp:112
- [ ] `l_do_active` — src\user\User.cpp:453
- [ ] `l_ready_pk` — src\user\User.cpp:466
- [ ] `l_find_player` — src\user\UserMgr.cpp:110
- [ ] `l_add_user` — src\user\UserMgr.cpp:118
- [ ] `l_log_trace` — src\util\Logger.cpp:179
- [ ] `l_log_debug` — src\util\Logger.cpp:180
- [ ] `l_log_warn` — src\util\Logger.cpp:182
- [ ] `l_log_error` — src\util\Logger.cpp:183
- [ ] `l_log_fatal` — src\util\Logger.cpp:184

## 2. 用户命令（cmd 非巫师）（2 条）

- [ ] `CCmdHandle::do_revive` — src\cmd\CmdHandle.cpp:344
- [ ] `CCmdHandle::do_kick` — src\cmd\CmdHandle.cpp:351

## 3. 角色（char）（16 条）

- [ ] `CChar::query_magic_exp` — src\char\CharAttr.cpp:61
- [ ] `CChar::query_magic_skill` — src\char\CharAttr.cpp:64
- [ ] `CChar::query_grant` — src\char\CharAttr.cpp:78
- [ ] `CChar::is_invalid` — src\char\CharAttr.cpp:101
- [ ] `CChar::attempt_apprentice` — src\char\CharFamily.cpp:73
- [ ] `CChar::send_off` — src\char\CharFamily.cpp:87
- [ ] `CChar::query_leader` — src\char\CharFamily.cpp:171
- [ ] `CChar::recruit_num` — src\char\CharFamily.cpp:206
- [ ] `CChar::skill2level` — src\char\CharLevel.cpp:41
- [ ] `CChar::reset_level_exp_cache` — src\char\CharLevel.cpp:63
- [ ] `generate_level_exp_approx` — src\char\CharLevel.cpp:73
- [ ] `char_rule_limit` — src\char\CharRules.cpp:12
- [ ] `CChar::clear_skill_prepare` — src\char\CharSkill.cpp:193
- [ ] `CChar::clear_skill_map` — src\char\CharSkill.cpp:195
- [ ] `CChar::clear_skill` — src\char\CharSkill.cpp:198
- [ ] `CChar::mini_skill` — src\char\CharSkill.cpp:261

## 4. 技能与战斗（skill / combat）（15 条）

- [ ] `~CCombatd` — src\combat\Combatd.h:43
- [ ] `CReward::distribute_party_exp` — src\combat\CReward.cpp:106
- [ ] `~CReward` — src\combat\CReward.h:25
- [ ] `CWeaponEffect::bash_weapon` — src\combat\WeaponEffect.cpp:17
- [ ] `CSkill::has_magic` — src\skill\Skill.cpp:47
- [ ] `CSkill::depends_on` — src\skill\Skill.cpp:59
- [ ] `CSkill::query_action` — src\skill\Skill.cpp:83
- [ ] `CSkill::query_attr` — src\skill\Skill.cpp:86
- [ ] `CSkill::reset` — src\skill\Skill.cpp:127
- [ ] `CSkill::query_dodge_msg` — src\skill\Skill.cpp:153 | 质疑 → 方案：入口通、只落脚本支——需补教材：一门不定义 query_dodge_msg 的闪避技能映射后被打，走基类默认台词
- [ ] `CSkill::practice_skill` — src\skill\Skill.cpp:159 | 质疑 → 方案：同样入口通只落脚本支——剧本新增一步 practice 一门未定义 practice_skill 的可 enable 技能，走基类默认增长
- [ ] `CSkill::valid_force_pro` — src\skill\Skill.cpp:173
- [ ] `CSkill::hit_ob` — src\skill\Skill.cpp:237 | 质疑 → 方案：dataset 只定义 hit_ob 字段、无调用且无绑定可调，虚调用全经 CLuaSkill → 基类体死码，考虑删除（保留 CLuaSkill 路径）
- [ ] `has_hit_ob` — src\skill\Skill.h:71
- [ ] `has_query_action` — src\skill\Skill.h:73

## 5. 物品与容器（item / object）（14 条）

- [ ] `CItem::do_use` — src\item\Item.cpp:116 | 质疑 → 方案：dataset 82 个 Can_Use 物品全部自带 do_use（0 反例），正常路径双侧死；唯一残余是脚本 pcall 异常兜底 → 评估删除，若留作异常兜底则列 §6 不测
- [ ] `CItem::query_max_amount` — src\item\Item.cpp:124
- [ ] `random_vendor` — src\item\ItemMgr.cpp:97
- [ ] `~CMCoin` — src\item\Money.h:29
- [ ] `CVendor::do_action` — src\item\Vendor.cpp:36
- [ ] `query_attr` — src\object\CObject.h:193
- [ ] `~CMainObject` — src\object\CObject.h:197
- [ ] `CContainer::query_entire_temp_dbase` — src\object\ContainerDbase.cpp:25
- [ ] `CContainer::query_short` — src\object\ContainerItem.cpp:60
- [ ] `CContainer::remove_call_out` — src\object\ContainerMisc.cpp:61
- [ ] `CContainer::query_max_amount` — src\object\ContainerWeight.cpp:63
- [ ] `CContainer::set_amount` — src\object\ContainerWeight.cpp:65
- [ ] `CContainer::add_amount` — src\object\ContainerWeight.cpp:67
- [ ] `CVector::get_buffer` — src\object\Mapping.cpp:538

## 6. 房间与副本（room）（10 条）

- [ ] `CDoor::bind` — src\room\Door.cpp:29 | 质疑 → 方案：绑定在册但教材无「改门向/动态门」场景，需补教材：一段 talk 改门向的动态门片段
- [ ] `CFB::do_look` — src\room\FB.cpp:61 | 质疑 → 方案：dataset 的 fb_registry 同样零出口，玩家 look 双侧够不到总控 → 考虑删除；若属 FB 接口强制实现则改占位保留不测
- [ ] `desc` — src\room\FB.h:48 | 质疑 → 方案：register.sce 游客登录链已在跑（rk_fb 无 desc → move_in 回退本函数），未命中新轮疑为空内联体被内联优化的采集口径问题，复核后移出清单（dataset 自己的 3 个 fb 均走脚本字段，与此不冲突）
- [ ] `reward_party` — src\room\FB.h:49
- [ ] `handle_action` — src\room\FBRegistry.h:17 | 质疑 → 方案：dataset fb_registry 零引用，四参分发双侧够不到 → 考虑删除；若属 IActionHandler 契约强制实现则改占位保留不测
- [ ] `CRoom::query_exits` — src\room\Room.cpp:236
- [ ] `CRoom::add_item` — src\room\Room.cpp:239 | 质疑 → 方案：与 l_add_item 同链（sample_guide 刷怪树、treebox.sce 在等），同条按「复核持久标记首触」处理，不改代码
- [ ] `CRoom::do_action` — src\room\RoomInterface.cpp:21
- [ ] `CRoom::download` — src\room\RoomInterface.cpp:70
- [ ] `CRoom::get_info` — src\room\RoomInterface.cpp:80

## 7. NPC（npc）（3 条）

- [ ] `CNpc::return_home` — src\npc\Npc.cpp:68
- [ ] `CNpc::create` — src\npc\Npc.cpp:86
- [ ] `random_npc` — src\npc\NpcMgr.cpp:85 | 质疑 → 方案：与 l_random_npc 同条——dataset `quest_mgr.lua` 已在用，照其精简移植成 sample 任务教材（商人为发布者）一并带出

## 8. 数据库（db）（10 条）

- [ ] `create_table` — src\db\IDatabase.h:61
- [ ] `set_charset` — src\db\IDatabase.h:78
- [ ] `db_query_relative` — src\db\SqlAccount.cpp:541
- [ ] `db_update_relative` — src\db\SqlAccount.cpp:571
- [ ] `~CSqliteDb` — src\db\SqliteDb.cpp:13
- [ ] `create_database` — src\db\SqliteDb.cpp:92
- [ ] `select_database` — src\db\SqliteDb.cpp:101
- [ ] `last_rows` — src\db\SqliteDb.cpp:113
- [ ] `error` — src\db\SqliteDb.cpp:115
- [ ] `IDatabase::create` — src\db\SqliteDb.cpp:135

## 9. Lua 桥与运行时（src\lua 非绑定）（7 条）

- [ ] `~CLuaActiveRefGuard` — src\lua\LuaEngine.h:367
- [ ] `~CLuaCalloutExecGuard` — src\lua\LuaEngine.h:382
- [ ] `lua_do_wield` — src\lua\LuaItem.inl:513 | 质疑 → 方案：与 l_do_wield 同条——dataset `Do_Wield` 在用（覆写 do_action 回落模式），sample 同件教材物品一并带出
- [ ] `lua_do_un_wield` — src\lua\LuaItem.inl:519 | 质疑 → 方案：同上，随 Do_Wield 教材一并带出（dataset 亦在用）
- [ ] `lua_do_action_base` — src\lua\LuaItem.inl:526 | 质疑 → 方案：同上，随 do_action_base 回落教材一并带出（dataset 亦在用）
- [ ] `CLuaSkill::base_query_action` — src\lua\LuaSkill.cpp:215 | 质疑 → 方案：被问的攻击技能全带 query_action——需补教材：一门不写 query_action 的高级攻击技能，练到上场打一次
- [ ] `CLuaSkill::has_query_action` — src\lua\LuaSkill.cpp:228 | 质疑 → 方案：只被 base_query_action 调用，随上一条同一次调用一并带出

## 10. 引擎 · 网络 · 基础设施（engine / net / user）（13 条）

- [ ] `CChannel::update_relative` — src\engine\Channel.cpp:93
- [ ] `CChannel::query_relative` — src\engine\Channel.cpp:98
- [ ] `CChatChannel::group_listener` — src\engine\ChatChannel.cpp:343
- [ ] `init` — src\engine\Condition.h:16
- [ ] `~CCondition` — src\engine\Condition.h:22
- [ ] `random_get_item` — src\engine\GameObject.cpp:20
- [ ] `wiz_level` — src\engine\GameObject.cpp:46
- [ ] `CRelationshipTracker::query_relative` — src\engine\RelationshipTracker.cpp:56
- [ ] `CRelationshipTracker::update_relative` — src\engine\RelationshipTracker.cpp:74
- [ ] `close_client` — src\net\GateClient.cpp:13
- [ ] `send_to_client_direct` — src\net\GateClient.cpp:49
- [ ] `CGateInfo::x_send` — src\net\GateInfo.cpp:102
- [ ] `CUser::user_dump` — src\user\User.cpp:223

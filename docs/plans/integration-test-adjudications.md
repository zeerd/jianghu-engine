# E2E 裁定台账（`integration-test.md` §6 的证据层）

> **分工**：`integration-test.md` §6「明确不测清单」只写**裁定结论与重开条件**（设计粒度，
> 每条一句话量级）；逐函数的公式推导、实测数据、行号证据、批次沿革（N1/N2/N4 之类代号）
> 与日期流水的家在**本台账**。§6 行内以「台账 §N」指路，本文件以「§6〈行名〉」回指。
>
> **改裁定时结论与证据同轮同步**：§6 某行改了，本台账对应小节必须跟着改（或删除）；
> 覆盖后从 §6 销账移出的条目，证据留在本台账作沿革，不回流 §6。
>
> **计划书不进本文件**：计划书是任务清单、用完即删；只有会被后来读者需要的甄别证据才落这里
> （分工铁律见 `.claude/rules/integration-test.instructions.md` §1）。

## §0 与未命中清单的关系

`run-integration-tests.ps1` 每轮把未命中函数与 `coverage_history/e2e-unhit-claimed.txt`
（决策认领清单，本机文件不入库）按清单原行匹配，分「决策认领 / 待推进」两段。三个各司其职：

- **结论**（为什么不测、何时重开）= `integration-test.md` §6；
- **证据**（当时怎么甄别的）= 本台账；
- **清单原行**（哪些函数被认领）= `e2e-unhit-claimed.txt`。

认领一批 → §6 一行（可按类合并）+ 本台账一节 + 清单逐行，三者同轮同步。

---

## §1 巫师命令域（§6 巫师行）

**N4 收口决策（2026-11-01）：不追求巫师命令覆盖度。** L9 引入的对象是**服务协议**本身
（需求道 `/t 1`+`tell` 请求 → 固定顺序 wait → 私发回执的请求/回执链——已由 10 道 E2E
全绿验证），巫师命令只是协议的工具面；服务链自然用到的 `full`/`set_skill`/`call` 已覆盖，
属**副产品**。余 38 条（Admin 14 / System 18 / World 6）**保持清单可见**
（CmdWizard* 留在排除表外、逐轮列示防盲区）**但不设消化目标、不计欠账**；
`snoop`/SnoopSystem 同不测（监听窥屏无剧本语义面）。

沿革：整域不测（旧决策）→ N4 立项移出排除表实测 → 收口定为「可见、不追求」。

---

## §2 `l_*` 教材未调用的内容需求批次（§6 `l_*` 行）

本行既定通道：**作者点名需求 → 教材补内容 → 剧本自然覆盖**（符合「教材是教材不是测试夹具」）。

**2026-11-01 内容需求批次兑现**：

1. 锁箱/钥匙（山脚锁箱 + `key.lua` + 房管命令 `lockbox`/`unlockbox`）——原记录「兑现 Box
   四条」**与代码不符，2026-11-02 订正**：`lockbox`/`unlockbox` 房管命令代码尚未落地，
   实际兑现 `CBox::locked`/`l_locked` 两条（`mountain_foot/locked_box.sce`）；
   `save_object`/`l_unlocked` 回落「待推进」，根因是**已点名内容未写完**，非可达性问题；
   **2026-11-02 B 批收口**：两条均已兑现、待推进段清零——`l_unlocked` 落点改道
   （作者裁定 house 不可改，`lockbox`/`unlockbox` 不落地；改由 `key.lua` 补 `do_use`
   双向锁 + `locked_box.sce` 中段「上锁→锁态遮蔽→开锁」），`save_object` 由 L1 独占片段
   `house/box_store.sce` 存取两支兑现（insert/update，卖房链 delete 支原有覆盖）；
2. 演武场（`sample_arena` 房 + `sample_arena_master` 教头脚本建房一条龙）兑现 FightRoom
   12 条（`get_team_count`/`l_select_member`/`l_can_join`/`l_join_team`/`l_begin_fight`/
   `l_set_single`/`l_end_fight`/`l_leave_team`/`l_is_fight_room`/`l_get_team_list`/
   `l_get_fight_team_num`/`l_get_fight_team_char`）+ 连带 `lua_check_char`；
3. 激励术收功分支兑现 `l_start_powerup`/`l_remove_powerup`（教材调用方落地，
   「零调用」前提已失效）+ 连带 `l_querymap_temp`。

**同类销账（原列 §6，测完移出）**：

- `l_combat_attack_unarmed`（Combatd.cpp:100）——2026-11-02 `sword_adv.lua` 的 `do_qi`
  改为「武器 + 空手」组合绝招，中段调用它；L2 `skill/sword_qi.sce` 以「剑招动作文本
  （`sword_adv` 出招表，60 级只落前三档）+ 掌招动作文本（`hand_adv` 出招表，10 级只落
  第一档「力劈华山」）」双断言分别钉住两半，负支另证空手半边的 enable/prepare 是真前置。
  空手半边的 AP/伤害按引擎约定走 `unarmed` 类别，故 L9 剑气供能另供 `unarmed 50`。
  §6 对应两行已同步收窄至 `l_combat_attack_weapon`（仍教材零调用，留认领）。

---

## §3 9 个「Lua→C++ 显式调用」胶水绑定的甄别实证（§6 9 胶水行）

N1 甄别实证：对「教材是否调用了某绑定」做文本检索时，`function M.key(...)` 的**字段定义**、
`-- hooks.key(...)` 的**注释**、以及 `book.create(...)` 这类**模块同名函数**都会命中 `key(`
形态——它们都不是对注入绑定的调用。判据应为「非 function 定义、非注释行的 `[.:]key(`」，
且**必须全量扫**（取前 N 处会漏掉后面的真调用）。N1 按此复扫后 17 个 D 候选切为
**9 个定义撞车**（并入「教材未调用」）+ **8 个真未触发**。

这 9 个绑定（`l_attempt_apprentice`、`l_create`、`l_die`、`l_do_channel`、`l_do_use`、
`l_init`、`l_prevent_learn`、`l_recognize_apprentice`、`l_room_reset`）的可走路径是
C++ 直调脚本字段/钩子（`C→Lua`），胶水方向天然无人走；0 命中与 `l_*` 教材未调用同性质。

---

## §4 N2-2d 载体/周期裁定（§6 `l_send_state`/`l_load_from_server` 行）

全库事实核查（2026-10-31），三子项：

1. `resource/fee` 原全库唯一设置 = 茶棚 **0**（`liquid_base` 记账分支 `fee>0` 永假），
   关联的 `l_add_balance`——**2026-11-01 内容需求批次改写**：作者点名「茶水收费」，
   茶棚 fee 定 10（营业场所收茶钱的世界设定），L1 剧本以「穷光蛋」负支 + 扣费成功支 +
   `/balance` 100→90 状态断言成对覆盖，`l_add_balance` 随之兑现，本子项作废；
2. `food_base` 与 `poison_base` **零继承者**（sample 食物/毒药自带实现，基类无人用）；
3. 房产缴费/离线 owner 重建（`house_base` 112-131、`HeartBeat::look_for_objects_to_swap`）
   双周期卡死——房间 reset **900 秒一轮**（E2E 窗口仅启动瞬间跑过、当时房未买）且计费
   `days×price/250` **按天**（单轮 days=0 直接 return）。

②③ 仍属**教材内容/调度与 E2E 时长尺度的结构性不交**，非剧本可达性问题。

---

## §5 N2-2a lua 域甄别 34 条（§6 N2-2a 行）

四子类归因（2026-10-31）：

1. **巫师/诊断域 14**：`/reload` 权限 80 命令的整条 bridge-script-notify 链、
   `dump_caller_info` 加载失败打印、`get_magic_name`/`display_name` **全库无调用方**、
   `lua_check_char` 内层助手其绑定教材未调；
2. **hook 读取端 5**：`result_bool`/`is_number`/`int`/`table_str_*` 全挂在**未注册 hook**
   后——`calculate_damage`、`can_prepare_combine` 不在注册面；
3. **教材未定义字段/4 参链 8**：`greeting`/`finish_kill`/`is_defeated`/`accept_fight`/
   `valid_force_pro` 无字段定义；`handle_action4` 四参链教材只给 3 参版、菜单走 3 参已命中；
4. **教材零调用胶水 7**：`lua_do_wield` 系与 glue 四 API——教材 grep 零调用。

与 9 个 N1 胶水绑定、`base_query_action`/`has_query_action` 回落链（教材 29 攻击技能全有
`query_action` 字段）同性质：**教材没用 / 入口不属 E2E 域**，非可达性缺口。

**D 组 5 条同轮核结追认**：LuaRoom 的 `create_int_impl`/`valid_leave_impl`/`desc_impl`
false_type 分支 = **标签分发死支**（traits 探测 C++ 方法、E2E 房间类型不走该支）、`reset`
转发 = 心跳 900s 轮从未跑；`LuaNpc hit_ob` 实证 = **山鸡出手 3 次仍不进**（野兽文案源自
CharFight.cpp:91 内置表 → NPC 攻击走老链不经 Attack 的 skill hit_ob 段）——**新老攻击链
并存的分流记为生产侧待察项**。

**C① 核结追认**：`can_combine_in_box`(inl) 需「有脚本 + CAN_COMBINE + 进箱」三门，E2E 箱内
四物唯一过堆叠门的 coin 是无脚本特型走基类版、玩家侧无 `put` 命令、教材无投放可堆叠脚本物
路径——结构性不可达（若未来加 put/战利品箱可重开）。

---

## §6 `l_query_team` 的结构性不可达与 0 歧义观察（§6 同名行）

**结构性不可达**（N2-2b 甄别）：同队检查（`throwing.lua:42`）之前有 33 行
`is_user(target)`「不能偷袭玩家」的 PvE 收窄——同战斗队成员必为玩家、被 33 行拦停；
唯一到达 42 行的路径是「我与目标（NPC）都在战斗中且 `query_team` 相等」，而未分队时
`query_temp` 返回 0 与 `FIGHT_TEAM_A=0` 同值（**0 歧义**）→ 0==0 意外相等误拒——此路径需
「NPC 混战」无自然触发面，且补测它的是 Lua 行（不进 E2E 覆盖统计）。C++ 侧
`CChar::query_team` 本体被战斗房链覆盖（L5 fight/pk）。

**0 歧义观察**（生产侧后续项，不属 E2E 批次）：`fight/fight_team` 的 0 同时是 `FIGHT_TEAM_A`
与「未设」，三个消费点受影响——`throwing.lua:42` 野外误拒、`CharState.cpp:243` 的
`if (query_temp(...)) tea=1` 让 A 队漏发战斗房标记、`CChar::is_fighting(enemy)`（仅
AutoFight 消费，§6 已排除）的普通战斗 `!=` 判定失效；处置留待战斗房语境专项评估，
此处只记录不修。

---

## §7 `l_is_fighting_target` 的内力经济推导（§6 同名行）

**内力经济结构性不可达**（N2-2b 甄别）：唯一教材调用点在 `force_adv.lua` 的 `do_roar`
（CAST_IN_COMBAT + **mp≥200** 硬门槛）。而引擎内力经济有硬约束——`exercise` 收工/halt
两条退出路径都把 `mp > 2×max_mp` 压回、收工 `mp ≥ 2×max` 时抬上限 1 点并 **mp 回落 max**
（`CmdStandardLife.cpp` 345/373/393），即**任何时刻 mp ≤ 2×max_mp**；max 唯一快捷来源是
经验果（+100，山顶树箱 **钉 L3 独占**）→ L3 `max=101`、**mp 封顶 202** < roar 200 +
sneak 100 = 300（不可共存，二选一即拿 do_roar 换掉已稳的 do_sneak 覆盖）；靠逐轮打坐抬
max 成本 ≈1 万拍；补第二段打坐编排需 **+181s → L3 521s 爆绝对预算 420**（关键路径禁加）。
L2 无果实、max 天花板更低。

**重开条件**：若未来出现内力载体（内力药/多果实），此条可重开。

---

## §8 拍板 B：`l_receive_wound` 与 `l_combat_attack*`（§6 拍板 B 行）

### 8.1 `l_receive_wound`——2026-11-01 破障实测（巫师 verbose 隔离实验）

把拍板 B 的旧推导修正为「**数学恒不可达**」：旧分析按 `mp=500 → −5+factor` 认为山贼 mp
可凑出 `damage<0`，**忽略了 `force.lua:39-41` 的 `mp_you ≤ 3mp` 封顶**——

```
damage = ⌊mp/30⌋ + factor − ⌊min(mp_you,3mp)/100⌋
```

由 `⌊3mp/100⌋ ≤ ⌊mp/30⌋` 恒成立（0.03 ≤ 0.0333）且 factor ≥ 1（`enforce` 拒负值，
CmdStandardMaster:576）→ **damage ≥ 1 恒成立**，三门中的首门（`damage<0`）任何合法取值都
进不去，后两门（徒手/随机）与命中面（实测破障可达：set_skill unarmed 300 起
P(命中)≥91%）都无从谈起。故本条由「降断言等概率收割」改为**明确不测**；
`force hit_ob` 的 C++ 调用点（Attack.cpp:364 三闸：mp_factor、mp≥factor、force 已激发）
随 L8 战斗走 ≥0 正常分支正常执行。

**重开条件（生产侧设计变更才有效）**：force.lua 的封顶或反震公式被设计修订、或出现负
mp_factor 的合法来源——均属游戏机制决策，**不为覆盖而改**。

**实测数据（2026-11-01 巫师 verbose 隔离实验，`env/combat=verbose` 打印 AP/DP/PP）**：

- 山贼 DP 49048.51（spawn `25+random(15)` 分布 ≈4.9万~8.4万）、PP = 2×DP（徒手攻方招架
  ×2）≈9.8万~16.9万，**PP 是命中瓶颈**；
- `query_skill(name)` 语义 = 自身折半 + 激发技全值（CharSkill.cpp:178/181），故
  `set_skill N` 生效 level = N/2，L8 镜像参（combat_exp 50000/str 10/dex 14）
  AP(N) = ((N/2)³/3+31000)/30×(12+N/10)，三点实测全中（存 0→15500 / 60→24000 /
  200→388622）；
- P(命中) = AP/(AP+DP)×AP/(AP+PP)：set 200≈71%、set 300≈91%、set 400≈97%——破障可达
  （set 300 起）但对覆盖无新增收益。

### 8.2 `l_combat_attack*`

唯一教材调用点 = `sword_adv.lua:94-98` / `throw_adv.lua:94-98` 的 `do_qi` 三连击，
硬门槛 = **adv 技能 ≥60 且 mp ≥100**（throw 版 mp≥80）；E2E 各道 adv 线每条仅 1~3 次
`/learn`（`learn_sword.sce:74`、`learn_basic.sce:36`、`learn_throwing.sce:17/69/88`）远低于
60，mp≥100 又受同条 `l_is_fighting_target` 的内力经济约束（§7）；`hooks.lua:30` 的另一处
只在注释里且所在 hook 未注册。

**重开条件**：出现「adv 技能 ≥60 + 战斗中 mp≥100」的供给源（N4 巫师 `set_skill`/`call set`
即此），届时按实测升回确定性断言。

**2026-11-01 兑现**：L9 剑气供能（`set_skill sword_adv 60` + `call set mp`）就位，L2
`skill/sword_qi.sce` 非战斗施放 do_qi（`check_ap` 对非战斗放行，L3 偷袭同款先例）命中
`l_combat_attack`；`l_combat_attack_unarmed` 已随 do_qi 组合绝招进入覆盖（2026-11-02，
记录见 §2 同类销账），`l_combat_attack_weapon` 仍教材零调用（do_qi 首尾走的是通用
`combat_attack`，按手中武器的 skill_type 自动解析），留认领——**重开条件**：教材出现
对 `l_combat_attack_weapon` 的直接调用（内容需求通道）。

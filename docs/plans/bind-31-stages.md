# §3.1 绑定演示面补全 · 分阶段计划书

> **性质**：阶段工具（任务清单），**用完即删**；禁止 `docs/design/`、`.claude/rules/`、剧本头注、脚本注释引用本文件（依据 `copilot-instructions.md` 铁律：计划书与设计文档的分工）。
> **格式**：待办条目 + 勾选 + 一行运行记录。论证与证据不写在这里，执行时回看本文件上方对话结论。
> **锚点行号**：以 2026-11-06 工作树为准，漂了按符号名找。

## 已拍板的三项裁定

- [x] 删 bind 分两批：**第一批 4 条**（`utf8_to_gbk`、`SaveToServer`、`drop_all`、`attempt_apprentice`），第二批 5 条（`wield_weapon`、`unwield_weapon`、`do_use`、`Offline`、`remove_doors`）
- [x] B2 三条：`query_apply` 原裁定「不删、补演示」，**2026-11-09 改判删除**（全仓零调用点，补演示动机只剩「补这个绑定的演示」，同 `t_record`/`ini_get` 撤回判尺）；`Offline`、`remove_doors` **删**（并入第二批）
- [x] 补 sample 先做**批次一**（双盲 5 条 + 点名的 `xrandom`），后续批次做完一段再开下一段

---

## 阶段 0 — 复核定档（0.5 天，不改生产/教材代码）

- [x] `docs\e2e-unhit-audit.md` §3.1 注销 `l_die_base`（教材已在用：`lua-sample\npc\sample_qingshe.lua:42`）
- [x] 同上，`l_random_get_item` 改判 A 档（数据集 `skill\base_skill\{alms,begging,stealing}.lua` 各 1 处 `RandomGetItem(`）
- [x] §3.1 总数 127 → **126**，按 **A 114 / B1 7 / B2 3 / B3 2** 分档标注，每条附数据集证据文件名
- [x] 给同名双向条目（`do_use`、`attempt_apprentice`、`die`、`create`、`init`）补「方向标注」：hook（C++→Lua）已演示 / 绑定（Lua→C++）缺失
- [x] 阶段 1 四条删除前终核：`l_utf8_to_gbk`、`l_save_to_server`、`l_drop_all`、`l_attempt_apprentice` 在 `src` + `lua-default` + `lua-sample` + `jianghu-dataset` 全仓零调用；注册表项唯一
- [x] 阶段 2 五条同上终核（`l_wield_weapon`、`l_unwield_weapon`、`l_do_use`、`l_offline`、`l_remove_doors`）；另核 `CRoom::remove_doors()` 除绑定外是否还有生产调用方（决定是否连带删）→ **结论：零生产调用方（`remove_all_doors` 是另一方法，被 `FBRoom.cpp:107`、`Room.cpp:30` 调用），阶段 2 连带删实现 + `Room.h` 声明 + `test_fb_room.cpp:182` 用例**
- [x] 运行记录：2026-11-06 — 三个 lua 树 5316 文件全扫，9 个键调用点均为 0；官方未命中清单核对：无 `l_die_base`（已转正），含 `l_attempt_apprentice`/`l_save_to_server`/`l_drop_all`/`l_utf8_to_gbk`/`l_random_get_item`；审计 §3.1 改写 127 行（126 标签 + 1 注销），BOM/LF 保全

## 阶段 1 — 删 bind 第一批：4 条（1~1.5 天，`src/` 生产代码）

每条固定动作：删 `l_*` 实现 → 删 `src\lua\LuaBind.cpp` 注册表项 → 删 `LuaBindDecls.h` 声明 → 同步删**绑定层**单测用例（C++ 方法层用例保留）。

- [x] `utf8_to_gbk`：`src\lua\LuaBindGlue.cpp:681`；删 `tests\unit\test_dispatch_encoding.cpp:81`；**C++ `utf8_to_gbk()` 与 `test_encoding.cpp` 保留**
- [x] `SaveToServer`：`src\char\CharSave.cpp:321`；注册表 `LuaBind.cpp:97`；**`test_char_save.cpp` 保留**（测 C++ `save_to_server()`）
- [x] `drop_all`：`src\cmd\CmdHandle.cpp:373`；删 `tests\unit\test_bind_char_misc.cpp` 4 处（249/361/386-393/405）；**`drop_all()` 自由函数与 5 处 C++ 调用方保留**
- [x] `attempt_apprentice`：`src\char\CharFamily.cpp:340`；删 `tests\unit\test_bind_char_family.cpp:106`；**`CChar::attempt_apprentice` 与 `test_lua_npc.cpp:296` 保留**
- [x] 理由口径统一写「被 X 取代 / 内部 API 不必暴露」，注释与提交说明不得出现测试、覆盖率、清单编号
- [x] 验收：`.\run-coverage.ps1`（delta ≥ 0）→ `run-cppcheck.ps1`（`bin\cppcheck.result.txt` 无问题）→ `.\run-integration-tests.ps1` 全绿（崩溃 0、未登记 ERROR 0、三行硬指标）→ `--check-lua` 过 `lua-sample` + `lua-default` + **`C:\jianghu-dataset`** 无残留引用
- [x] 运行记录：2026-11-06 — ① `run-coverage.ps1` 全绿（engine 2258/2258 + cli 278/278，94.7%，delta +0.0%）；② `run-cppcheck.ps1` 零问题；③ `run-integration-tests.ps1` **PASS**（10 道 + persist 两段 + reload 全过，未登记 ERROR 0，223s/420s 预算，关键路径 L3 220s，覆盖率 src 68.7%，未命中清单 **249 → 245**，恰好 −4）；④ `--check-lua`：`C:\jianghu-dataset` **5216 OK / 0 FAILED**、`lua-sample` 52 OK、`lua-default` 48 OK。**计划外修正 2 处**：`test_bind_char_combat.cpp:491` 的 `l_save_to_server` 残留（首轮测试 grep 漏小写变体）改直调 `u.save_to_server()`；`test_bind_char_misc.cpp` 的 `DropAll_InventorySweep` 改直调 `drop_all(&me)`（保 `drop_all()` 行为覆盖），另删 3 个纯绑定守卫用例并补 `#include "CmdHandle.h"`

## 阶段 2 — 删 bind 第二批：5 条（1 天）

- [x] `wield_weapon` / `unwield_weapon`：`src\char\CharCombat.cpp:307/327`；删 `tests\unit\test_bind_char_combat.cpp:541-603`；`item:wield()` 为保留的更优解
- [x] `do_use`：`src\item\Item.cpp:279`；删 `tests\unit\test_bind_global.cpp:1499`、`test_bind_item.cpp:54`；**hook 方向 `CLuaItemT::do_use` 与 `command("use")` 链不动**
- [x] `Offline`：`src\char\CharSave.cpp:332`；删 `tests\unit\test_bind_char_combat.cpp:503`；**`CUser::offline()` 保留**
- [x] `remove_doors`：`src\room\Room.cpp:408`；删 `tests\unit\test_bind_room.cpp:95-96`；`remove_all_doors` 绑定与 `CRoom::remove_doors()` 按阶段 0 终核结论处置 → **连带删 `CRoom::remove_doors()` 实现 + `Room.h:51` 声明 + `test_fb_room.cpp:175` 用例**
- [x] 验收同阶段 1（四道全过，含以 `C:\jianghu-dataset` 为目标的 `--check-lua`）
- [x] 运行记录：2026-11-06 — ① `run-coverage.ps1` **全绿**（engine 2253/2253 + cli 278/278，94.7%，delta +0.0%）；② `run-cppcheck.ps1` 零问题；③ `run-integration-tests.ps1` **PASS**（10 道 + persist 两段 + reload 全过，未登记 ERROR 0，220s/420s，覆盖率 src **69.1%**，未命中清单 **245 → 237**）；④ `--check-lua`：`C:\jianghu-dataset` **5216 OK / 0 FAILED**、`lua-sample` 52 OK、`lua-default` 48 OK
- [x] **计划外修正 1 处**：首轮 `run-coverage` 报 3 个失败（`CharSkillTest.ApplyCondition_*` 单跑也失败、`CmdStandardCombatTest.Cast_InheritedRecover` 仅批量失败）。根因是**测试间依赖**被分片重组暴露——`CharSkillTest` 夹具没建 Lua 环境，而 condition 已全 lua 化（`CLuaCondition::init` 找不到脚本就拒绝挂载）；s1 分片里它排第 3，前面无人调 `ensure_lua_env()`。修法按 `unit-test.instructions.md`：夹具 `SetUp()` 自建环境（`ASSERT_NE(ensure_lua_env(), nullptr)`），连带让后序 `Cast_InheritedRecover` 转绿（原先是被中段测试用非测试根初始化污染）

## 阶段 3 — 补 sample 批次一：双盲 5 条 + `xrandom`（2~3 天）

> 每条 = sample 改动（注释只写「该 API 演示什么机制」）+ 一段 `.sce` 片段（新文件，`lane_*.sce` 只写 include）。只加 sample 不加片段对 §3.1 零消解。

- [x] `base_attack` → `lua-sample\npc\sample_iron_dummy.lua` 开战后的回合还手（attack 钩子在其己方回合派发、一回合一击，非受击额外攻击）；改完回归 `skill\mirror_dummy.sce` ✓ run3 全绿
- [x] `Remove_Betrayer` → `lua-sample\npc\sample_master.lua`「悔过」菜单（`do_talk` 注册问话 + `handle_action` 两支：有记录清记录 / 无记录拒绝）；置位面按裁定新加**黑风寨门派**：`sample_heifeng_master.lua`（黑风寨总教头·落山顶·`do_talk`「判师」，绕行命令层改派闸 + betrayer 记账），零情节；独立片段 `social/betray_and_repent.sce` 挂 L4 道尾（山脚→山顶判师→半山腰悔过），`attempt_apprentice` 主链未动 ✓
- [x] `query_dex`（连带 `query_str/con/int/per`、`query_combat_exp`、`query_valid_exp`）→ 属性查询菜单，私发文本可断言：`sample_master`「验看」问话 → **门规四问**（str/con/int/dex 合格线 20，回绝支不到场、报数行出现即四问全过）+ **实学报数**（历练 = `query_combat_exp`、可用经验 = `query_valid_exp`，断正常玩法攒出的精确值 51000/49700）+ **容貌比较**（`query_per` 与老夫各读一次，老夫 create 钉 0 弟子恒胜）。片段 `social/master_inspect.sce` 挂 L4 投票之后（历练 = 大考 1000 + 问路 50000、可用经验 = 51000 − level2_exp(5)=1300 升级已耗，`wait attr combat_exp == 51000` 与报数两源互证）。连带 `l_query_valid_exp` 由 CNpc 收窄放宽回 CChar 域（`upgrade()` 本就对玩家调此方法），绑定单测收紧为精确断言
- [x] ~~初版返工~~：初版是数字罗列菜单 + 断 0/断 NPC 静态值（`可用之学`/`福缘` 两个生造词），经裁定为无真实测试意图的覆盖率填充——重做为上述门规判定 + 正常玩法状态断言；`xrandom` 片段的奖品断言同轮改为奖池 13 件全列 or 串
- [x] `xrandom` → `item/sample_blind_box.lua` 拆盒抽奖的 E2E 片段 `mountain_foot/blind_box.sce`（盲盒 sample 上轮已落地，只加 sample 不加片段对 §3.1 零消解，本轮补齐）挂 L1 商人采购之后
- [x] 新场景挂非关键路径道（design §2.5，名单以当轮报告三行硬指标为准）→ 验看在 L4（103s）、盲盒在 L1（26s）；终轮关键路径 **L3 山顶战斗尾 263s**，两道均不在其列
- [x] 验收：stylua 全量 + `jianghu.exe --check-lua --dir lua-sample`（54 OK）→ 本轮动了绑定面，数据集按 `copilot-instructions.md` 的姿势（`cd bin` + `--config ..\data\config.ini`）复刻 validate 第 2 关：**5216 OK / 0 FAILED** → `run-cppcheck.ps1` 零问题 → `.\run-coverage.ps1` delta +0.0% → `.\run-integration-tests.ps1` 全绿 + 三行硬指标 → 更新 §3.1 对应勾选（本轮勾 8 条：query 七条 + `xrandom`）
- [x] **计划外修正 1 处**：盲盒 `/t mang he` 撞同步竞态——`CVendor::do_buy` 先发回执后 `move`（`Vendor.cpp:207→210`），`/t` 抢在背包同步之前落「Target not found」（首跑侥幸过、复跑必红）→ 买后加 `wait item 盲盒` 条件同步，兼作「真进背包」状态断言（§8.1 条件同步 / §8.4① 能用状态就别只看回执）
- [x] 运行记录：2026-11-06 — ① stylua 全量无残留；`--check-lua` 教材 **54 OK / 0 FAILED**、数据集 **5216 OK / 0 FAILED**（绑定面改动，validate 第 2 关姿势复刻）；② `run-cppcheck.ps1` 零问题；③ `run-coverage.ps1` 全绿（engine 2253/2253 + cli 278/278，94.7%，**delta +0.0%**，CharAttr 行覆盖 164→166——放宽支被收紧后的单测吃住）；④ `run-integration-tests.ps1` **PASS**（10 道 + persist 两段 + reload 全过，未登记 ERROR 0、崩溃 0，**265s/420s 余量 +155s**，关键路径 **L3 263s**；L1 358 steps、L4 401 steps；覆盖率 src **69.8%**，未命中 **225 持平**——上轮出列的 8 条不回潮）；⑤ 返工两轮红已闭环（见「初版返工」与「计划外修正」两条）

## 阶段 4 — 补 sample 批次二：战斗状态簇 G2（2026-11-07 收口）

- [x] `unconcious` + `revive` → 新增 `condition/drunk.lua`（四拍：播报 → 逐拍递减 → 拍到 1 `unconcious()` → 拍到 0 `revive()` 并回满气血；苏醒归状态自己管，赶在 `unconcious` 同时起的 30~109 拍自动苏醒兜底之前）+ 出厂即满的 `item/sample_jiu.lua`（商人货位 14，走 `liquid_base.do_use` 的 alcohol 分支——装清茶的酒壶走不到这条）；片段 `mountain_foot/drink_and_faint.sce` 挂 L1，全程山脚不挪窝（昏过去期间任何命令都被禁用操作挡掉）
- [x] `set_killer` / `fight_ob` / `is_killing` / `remove_all_killer` → 新增死胡同房 `room/sample_woods.lua`（山脚单门、不设 no_fight）+ `npc/sample_road_rogue.lua`；片段 `mountain_foot/road_rogue.sce` **以 kill/fight 两条正常命令为主轴**（裁定：`set_killer` 本意是标记「`/kill` 进战斗还是 `/fight` 进战斗」，进房即宣死战属表达恶意的特殊用法，不走）——`/fight` → `accept_fight` → `do_bishi` 比试房，`notext 想杀死你` 盯住它不碰名单；`/kill` → 双向 `set_killer`，私发「想杀死你」。**脚本侧按话头分岔（裁定：发起战斗靠对话，清除敌意才靠贿赂）**——`do_talk`「挑衅」读 `self:is_killing(me)`（`duanjia_sword.lua:190` 同向）分真假支：首次 `set_killer` 记账、名单已在报出真支；**`set_killer` 不开打**，只把人写进追杀名单，故同一句可反复问、话一直说得下去；`do_talk`「讨教」→ `fight_ob` 还手不记账，**排在最后一句**，说完即开战、之后不再需要说话；`accept_object` 收受贿赂 → `remove_all_killer` 双方销账，**排在开战之前**（非战斗态 give 不走 30 AP 门）；**销账后再问同一句「挑衅」翻回假支，即销账的状态证明**
- [x] `receive_poison` → `condition/snake_poison.lua` 逐拍毒伤改走本绑定（落点由 hp 改为 wound 语义：扣 eff_hp 并把当前气血压到其下），`social\poison_and_cure.sce` 断言不动
- [x] `killer_reward` → **落在新野怪而非原计划的青蛇/山贼**：青蛇 `die` 回链 `die_base` 已由 C++ 结算一次，再调即重复入账，且动它要碰 L3 关键路径；改由 `sample_road_rogue` 的脚本接管死亡。配 `nk_bonus` 固定击杀经验——`nk_gain` 的历练折算会把杂鱼折成 0（L1 历练 53000 → `dx≈23555`，35 点折完为 0，`reward<=0` 静默不发），固定值只能走 `hooks.kill_bonus`，这正是该挂点的契约
- [x] `damge_to_string` + `combat_attack_weapon` → `data/hooks.lua` 的 `hooks.fight` 棋子补击（蓝本即数据集 `hooks.lua:292`），flag 走 `TYPE_HIT` 必中档；**补击本身以招式文案「扬起棋子…」断言**——它由 `build_action_description` 出、排在闪避判定之前，只要补击发出必然可见；伤害文案挂在 `hooks.calculate_damage`（伤害落到身上之前，`damage` 即实打数值），打在山道强人身上的每一击格式化后私发给出手方（返回 nil 不改写伤害）
- [x] `Do_BiShi` → `sample_arena_master` 加「指教」问话（与既有逐绑定装配链并存，`check=90` 阈值血线收场），`interaction/arena.sce` 段尾
- [x] `MakeCorpse` → 同 `killer_reward`，在 `sample_road_rogue` 的 `die` 里自造尸体（不回链 `die_base`，否则奖励与尸体交回 C++ 再做一遍）
- [x] 验收：stylua 全量 ✓ → `--check-lua --dir lua-sample` **59 OK / 0 FAILED** ✓ → `run-integration-tests.ps1` **PASS**（10 道 + persist 两段 + reload 全过）✓ → `run-cppcheck.ps1` **0 问题** ✓ → 未命中清单 **225 → 210**、§3.1 勾 12 条 + §2.1/§5.2 盲区 2 条 ✓
- [x] **计划外修正 13 处**：① 醉酒苏醒的 `receive_healing` 只改服务端 dbase，客户端 `attr hp` 一直读 0 → 补 `FlushMyInfo()` 才推得动（蛇胆同款推法）；② 暗器被闪避——无 dodge 技能时 `dp = combat_exp/2 ≈ 15500`、被闪 86.5%，而 `RESULT_DODGE` 会让 `resolve_parry`/`calculate_damage` 整段跳过、文案钩子不跑 → 改 `TYPE_HIT` 把 `dp/pp` 压到 1（被闪 1/2428）；③ 初版把 `set_killer` 当「NPC 表达恶意」用（进房即宣死战），经裁定为特殊用法 → 重做为 `/kill` vs `/fight` 两条正常命令的分野 + 脚本只读 `is_killing`；④ `end_fight` 把人送回空地会重触发脚本 `init` 的初版实现，原「离房再回」段会把人拽进第二场战斗房出不去——随③一并消失（脚本已无 `init`）；`yield` 保留一次性闸防第二场收手抢在死亡之前；⑤ 战斗中 `/ask` 被 `CCmdUser::talk` 的 `is_fighting` 分支挡死（只有 NPC 名「霜儿」例外）→ 三问全部移到战斗外；⑥ `attack` 回合计数补 `is_killing` 守卫，否则切磋场也数到第 5 拍被提前收手、破坏 `/halt` 流程；⑦ 伤害文案原挂 `hooks.calculate_damage`，而它在闪避后整段跳过（3 轮仅 1 次补击且文案未达）→ 补击本身改断排在闪避之前的招式文案「扬起棋子」，`damge_to_string` 改挂「打在山道强人身上的每一击」（玩家攻击实测每轮必中）；⑧ **既有结构隐患**：`lane_foot.sce` 里 `blind_box` 排在 `locked_box` 前，而盲盒奖池 13 件含铜钥匙，抽中即让「空手 open 被拒」支不可达（约 1/13 必红）→ 两段对调；⑨ **`CChar::is_killing` 真 bug**：`set_killer`/`remove_killer` 以 `querystr("id")` 为键、`is_killing` 却拿 `object_id` 十进制串去 `find`，同一名单两套口径 → 读侧恒假，连带 `CChar::init` 的 hatred 自动开战、`begin_fight` 的仇人入伙、`follow_me` 追杀三条生产分支结构性不可达 → 统一到角色 id；**4 处既有单测原本在给它打补丁**（`EXPECT_FALSE(is_killing)` 断言 bug 恒假、把 `id` 改成 object_id 串绕行、注释「不用 is_killing」），一并改回正常断言并新增按 id 精确匹配的用例；⑩ `yield` 的「离场」收不了场——`CFightRoom::select_member` 只看 `m_Team` 槽位、不看所在房间，人走了战斗照打 → 改为下一拍 `End_Fight()`（`call_out` 出回合，避免回合中途拆房）；⑪ 场3 的 `notext 想杀死你` 立即失败——`notext` 扫的是 `recent_output(2000)` **整个缓冲**、不是从本步起的窗口，真支回敬 `set_killer` 的私发还留在里面 → `clear` 放在**假支 `/ask` 之前**（放之后会把 `fight_ob` 若真发出的那句一起抹掉，变成假绿）；⑫ **既有结构隐患**：`treebox.sce` 的树箱护栏 240s 只覆盖约 48 次掷、失手概率约 0.4%，护栏落在分布内侧就会随机变红 → 提到 420s（约 88 次掷、失手 0.01%）——护栏的职责是拦「刷箱链断了」，不是给掷骰定统计上界；⑬ **⑨ 复活的雷打穿 L2**：`/kill` 写下的名单在玩家每次进房时都由 `CChar::init` 的 hatred 分支重排一颗 5~10 秒后引爆的自动开战，`powerup` 的两处 `sleep` 正落在窗口里、战斗中 `/cast` 撞「需要…AP」→ 把 `iron_dummy_retaliate`（本道最后一处 `/kill` 且铁人桩不死）移到 `powerup` 之后、`skill_admin` 之前；`prepare_combine` 打散的练功木人随 `die` 自毁、它那份名单一并不留，故 `powerup` 处在零名单状态；L9 的固定消费顺序（剑气→组合→激励）一字未动；⑭ **结构简化·第一轮（被否）**：`talk` 战斗中被挡是那 38 行脚手架（`attack` 数拍 / `yield` 认栽 / `settle_yield` 压血 + `call_out` 延后 `End_Fight` 拆房）存在的**唯一**原因，遂改用 `give` 收礼触发（`do_talk` → `accept_object`，脚本 6 函数 144 行 → 3 函数，E2E 一度全绿）。**否决理由：给东西让人动手，语义不成立，那是绕开 talk 挡板的凑覆盖率——清除敌意用贿赂说得通，发起战斗不能用**；而且战斗内 give 每笔要花 30 AP，反倒多出一整块攒 AP 的脚手架。⑮ **结构简化·归位（终稿）**：按引擎语义拆触发器——`set_killer` 本身**不开打**（只把人写进追杀名单，下次进房由 `CChar::init` 的 hatred 分支自动开战），所以「挑衅」能反复问、话一直说得下去；`fight_ob` 才开战，排在**最后一句**「讨教」，之后不再需要说话、一路打到死；`remove_all_killer` 走贿赂（give）且排在开战之前——非战斗态不走 30 AP 那道门，攒 AP 块整个消失。结果：脚本 6 函数 144 行 → **4 函数**、零收场脚手架、话全在战前说完；配套 `max_hp` 2000→200（按玩家实测 4s/击 ×24 点配平；比试房 50% 判负线要四击，而场1 只打一击够不着）、`buy_sell` 金创药回退 3 包（贿赂走金币，启动本金自带）
- [x] 运行记录：2026-11-07（收口轮，**连续两轮 PASS**——16:10 210s、16:25 222s，同码复跑以确认对话版设计非侥幸） — ① `run-coverage.ps1` 全绿：engine **2254/2254** + cli **280/280**，Overall **94.7%**，**delta +0.0%**（`is_killing` 修复分支由更新后的单测吃住，`char\Char.cpp` 93.2%）；② `run-cppcheck.ps1` **0 问题**；③ stylua 全量 ✓、`--check-lua --dir lua-sample` **59 OK / 0 FAILED**；④ `run-integration-tests.ps1` **PASS**：10 道全过 + persist 两段 + reload，未登记 ERROR 0、崩溃 0；**222s / 420s 余量 +198s**，关键路径 **L3 山顶战斗尾 219s**、次长 L8 215s；L1 **102s / 411 steps**（`road_rogue` **55.1s / 41 步**，对话双支 `记下了`→`新账旧账` 均命中），离关键路径差 117s；未命中清单 **916/1126、未命中 210**，与前轮**逐项一致、零新增**。⑤ **覆盖率 src 70.9%（11124/15680），较 give 版基线 11131 少 7 行**——同码复跑得 11123/11124，说明该指标噪声仅 ±1，故这 7 行是**系统性损失**，来自两处主动删改：`attitude` `aggressive`→`killer`（拆掉会炸问话窗口的雷）与 `fight_ob` 从两次减到一次（删掉战斗内那次空转）。**函数级零退化；要补回就得把刚被否掉的东西加回去，不补。** ⑥ Δ +12s 超参考阈值 10s（§3.4 仅诊断不判失败）：L3/L8 +11s、L10/L2/L9 +17s 而 L5/L6/L7 −11s、各道步数不变，属全局调度相位漂移
- [x] **附带观测改动（非 §3.1 范畴，为定位「墙钟去哪了」而加）**：CLI 剧本执行器打分场景计时——回显加 `[+-]秒` 前缀、跨剧本打 `[timing]` 边界、收口打 `[timing]` 汇总；`run-integration-tests.ps1` 每轮把 `test_tmp\e2e_lanes\*.log` 归档到 `tests\integration\log\lanes-<runid>\`（旧轮日志会被覆盖，尖峰复盘全靠它）。`run-coverage.ps1` 全绿：engine 2253 + cli 280（新增 2 条计时契约测试），delta +0.0%
- [ ] **观察项（非本阶段引入，留待后续）**：L5↔L6 邀请菜单竞态——A 的邀请早到、落在 B 尚未走到 `invite_b` 的窗口，B 后续 `cmd /t 0` 会冲掉 CLI 的菜单状态，`wait menu` 干等 60s；连锁使 L9 卡在「冲级供能请求」367s、L2/L3/L4/L8/L10 全部 300s 超时（曾出现一轮 10/10 全挂，复跑未复现）

## 阶段 5 — 补 sample 批次三：技能 / 师徒 / 物品（G4 / G3 / G5，3~4 天）

- [ ] G4：新增「家传武学书」物品（`query_hardcore_skills` → `inc_skill` → `load_skill` 一条链）；师父「传功」`improve_skill`；「遗忘」`delete_skill` + `clear_skill*`（一次性 NPC，不动道上角色技能）；`FlushMySkill`、`clear_condition`、`reset_level`、`valid_force`
- [ ] G3：`sample_master`「门规」查询串 `get_room_char` + `is_relative` + `recruit_num` + `query_recruit`（只读，兼容 L5/L6 收徒链）；`prevent_learn` 掌门拦截分支
- [ ] G5：新增面具/斗笠物品（`Do_Wield` + `Do_UnWield` + `do_action_base` 三件套）；`Split` 分银；`Query_Amount` / `Query_Weight` / `can_equipped` 挂现有堆叠食物与装备动作；`get_map` / `set_map` 存钱罐；`queryvec` / `querymap_temp_keys` 装备槽枚举
- [ ] 验收同阶段 3
- [ ] 运行记录：

## 阶段 6 — 补 sample 批次四：系统 / DB / 房间（拆 6A~6D，可独立执行）

> 已拍板三项裁定（2026-11-08）：
> ① `sql_update_record` **补演示调用**——原 DB 链勾选含它但样例树零调用（§3.1:286、§2.1:112、§5.2:520 三处未勾/未出列），归 6A；**2026-11-09 撤回**（落地后复核不成立，见 6A 条目）；
> ② `message_channel`（读书上频道）**挂起待调查**——落点存疑，本阶段不做、不勾，原条目保留在 6A 备注里；
> ③ `query_apply` **已改判删除（2026-11-09，见 6B）**，原「依赖阶段 5 的 master_inspect 同文件改动」的排期作废；其余子阶段与阶段 5 无耦合。
> 每子阶段独立验收（同阶段 3）+ 独立运行记录，§3.1 勾选随各自子阶段走；子阶段间无顺序依赖（6B 除外，只需晚于阶段 5）。

### 阶段 6A — 系统小件：DB 台账收尾 + 中文数字 + hooks 私发（0.5~1 天）

- [ ] ~~`sql_update_record`：`data/box_draw_log.lua` 写 `t_record` 一条开盒记录 + `db_query` 回读报数，并入「此盒已记档」回执~~ **撤回（2026-11-09）**：落地后复核立不住——开盒记档与引擎历练记录表 `t_record` 在本世界没有关系，调用动机只剩「补这个绑定的演示」，答不出「教材读者学它干什么」；且链路只走 insert 支，「档位只升不降」在示例里无实例。教材 / 剧本 / README / 台账已全部撤除，该绑定回到 §3.1 待演示，**等世界出现「按账号记进度、只升不降」的真机制再谈落点**
- [x] `chinese_number`：`data/exam.lua` 答对播报改 `chinese_number(REWARD)`（1000 →「一千点」）；`social/channel_and_banword.sce` 加 `wait text 一千点`（保留 `答对了` 断言；`master_inspect` 的 51000/49700 精确断言不动——奖励数额没变）
- [x] `message_vision_self`：`data/hooks.lua` 加 `hooks.post_buy`（蓝本 `hooks.lua:739`，只私发双方不进房间广播），**收窄到盲盒商品、只 L1 触发**（否则全道买货都冒私语、污染他道 `wait`）；`mountain_foot/blind_box.sce` 加一行 `wait text`
- [ ] ~~`ini_get`：选定交互回显 `ini_get("server", …)` 实际值（落点执行时定，候选：向导问路 / 验看回执尾）；断测试 config 已知值（如 `name = 金剑江湖`），教材侧回显动态值不写死~~ **演示撤回（2026-11-09）**：原落点「验看回执尾」立不住——那句「此界名号」不属于验看的门规/实学/容貌任何一环，世界里也无人消费「世界名号」，动机只剩补演示（与 `t_record` 同判，A1 处理）。**保留 `l_ini_get` 的编码契约修正与单测**（`IniGetBindNormalizesConfigValueToGbk`：配置文件是 UTF-8、Lua 字符串空间是 GBK、绑定层是唯一边界——修复理由是设计，与有没有人演示它无关）；该绑定回到 §3.1 待演示，**等世界出现「部署可调参数」或「文案引用配置」的真需求再谈落点**
- [ ] ~~`message_channel` 读书上频道~~ **挂起**（裁定②，待调查后另归阶段），本子阶段不做
- [x] 落道按当轮三行硬指标选非关键路径道；lane 文件只加 include 行（§8.7）→ **零 lane 改动**：两条都落既有片段（L1 `blind_box` 的成交须知、L4 `channel_and_banword` 的大考播报），当轮关键路径 L3、次长 L8 都不含新增步骤（`master_inspect` 随 `ini_get` 演示撤回回到前态）
- [x] §3.1：勾 2 条（`chinese_number`/`message_vision_self`）+ **补勾 DB 已落地四条**（282 `db_execute`、283 `get_db_type`、284 `db_register_ddl`、285 `db_query`——代码在 `box_draw_log.lua` 早已落地、台账没勾）；`sql_update_record` 三处（§3.1:286、§2.1:112、§5.2:520）与 `l_ini_get`（§3.1:299）随撤回**回到未勾**（§2.2 盲区② 四条随 §3.1 同步勾）
- [x] 验收同阶段 3
- [x] 运行记录：2026-11-09 — ① `run-coverage.ps1` 全绿：engine **2255/2255** + cli 280/280，Overall **94.7%**，**delta +0.0%**（含本轮新增 `ini_get` 编码归一单测）；② `run-cppcheck.ps1` **0 问题**（123 文件）；③ `style.ps1` 全量跑过（clang-format 0 dirty、三棵 lua 树无 BOM、stylua 通过——仅把在途的 `sample_heifeng_master.lua` 一处调用折回单行，属格式不涉逻辑）；④ `--check-lua`：教材 `lua-sample` **60 OK / 0 FAILED**、`lua-default` **48 OK / 0 FAILED**、数据集 **5216 OK / 0 FAILED**（`bin\jianghu.exe` 被 6600 端口功能测试服锁住，按进程安全规则不碰，改用 E2E 自建的 `bin\e2e\jianghu.exe`，cwd=bin + `..\data\config.ini` 同 validate 第 2 关姿势）；⑤ `run-integration-tests.ps1` **PASS**：10 道 + persist 两段 + reload 全过，未登记 ERROR 0（白名单 22）、崩溃 0；**210s / 420s 余量 +210s**（较前轮 224s **−14s**，未超参考阈值），关键路径 **L3 208s**、次长 204s，新增场景所在 L1/L4 均非关键路径；覆盖率 src **71.5%**（11192/15658，前轮 70.9%）；未命中 **922/1125、未命中 203**（前轮 916/1126、210）——本阶段点名出列 5 条（`l_sql_update_record`、`sql_update_record`、`l_ini_get`、`l_chinese_number`、`l_message_vision_self`），总数 −1 为在途改动删除 `l_query_hardcore_skills`，余 1 条命中随在途判师散功链转绿，**本阶段改动零新增未命中**（只动了 `l_ini_get`，已转绿）
- [x] **撤回复跑（2026-11-09，`t_record`；上一条运行记录中的出列 5 条内 `l_sql_update_record`/`sql_update_record` 两条随之失效）**：删净 `t_record` 相关内容——`box_draw_log.lua` 还原至 HEAD（零 diff）、`sample_blind_box.lua` 记档回执还原、`blind_box.sce` 撤 `l_sql_update_record` 覆盖条目与档位断言、README 两行还原、台账三处回退未勾（§2.1:112、§3.1:286、§5.2:520）。复跑：① `run-coverage.ps1` **2255/2255 + cli 280/280**、94.7%、**delta +0.0%**；② `--check-lua` 教材 **60 OK / 0 FAILED**（数据集 5216 OK 仍有效：未动绑定注册表）；③ `run-integration-tests.ps1` **PASS**：**231s / 420s 余量 +189s**，关键路径 L3 229s、次长 L8 225s；**Δ +21s 超参考阈值 10s（§3.4 仅诊断）**——各道步数与前轮一致（L3 143、L4 403、L8 259），属全局相位漂移；src **71.4%**（11175/15658，`db` 段随演示撤除 −27 行）；未命中 **922/1125、203 持平**——`l_sql_update_record` 随撤回归清单（点名），另 1 条轮间转绿抵消（上一轮清单已被覆盖，无法点名，属轮间噪声）
- [x] **撤回复跑·A1（2026-11-09，`ini_get` 演示撤回）**：`sample_master.lua`（验看收尾报世界名）与 `master_inspect.sce`（覆盖/判定/并行注释 + `wait text 此界名号`）**还原至 HEAD**，全仓 `.lua`/`.sce` 已无 `ini_get`/`此界名号` 残留；§3.1:299 回退未勾；**保留** `l_ini_get` 编码契约修正与单测。复跑：① `stylua` ✓；② `--check-lua --dir lua-sample` **60 OK / 0 FAILED**；③ `run-integration-tests -SkipBuild` **PASS**（10 道 + persist 两段 + reload，未登记 ERROR 0、崩溃 0）：**215s / 420s 余量 +205s**、关键路径 L3 213s、次长 L8 209s、**Δ −16s 未超参考阈值**、L4 **402 steps（−1 = 撤掉的那条 wait）**。⚠ 两件事同因：另一会话正在源码树就地删除 `sql_update_record`/`t_record`（本轮时刻：`DbTables.cpp:258/267` 建表调用删残、`FB.cpp:185` 调用方未随改，`test_db_layer`/`test_sqlite_integration`/`test_bind_global`/`test_fb_room` 共 10 处既有用例未处理）——`run-coverage` 因此**构建红**（非本阶段改动所致，我的 C++ 自 16:34 全绿那轮后未变）；E2E 改用 `-SkipBuild`（16:34 的 exe，剧本/脚本取当前最终态）跑通**剧本门禁**，但该轮**覆盖率与未命中清单受源码/二进制行号错位影响不作台账依据**（征兆：分母 15658→15598、`SqlRank.cpp` 逐文件数据消失、清单出现 `SqlRank.cpp:78 l_paihangbang` 行号漂移）。**收口（同日 17:0x，源码树恢复可编译后补跑，四道全绿）**：① `run-coverage.ps1` **engine 2246/2246 + cli 280/280**、Overall **94.7%**、**delta +0.0%**；② `run-cppcheck.ps1` **0 问题**（17:11 重跑）；③ `run-integration-tests.ps1`（完整带构建）**PASS**：10 道 + persist 两段 + reload、未登记 ERROR 0、崩溃 0，**213s / 420s 余量 +207s**、关键路径 L3 210s、次长 L8 206s、**Δ −2s 未超阈值**、L4 **402 steps**；本轮源码/二进制一致 → 覆盖率 src **71.5%（11164/15611，分母随并发改动 15658→15611）**、未命中 **921/1124、203**：`l_ini_get` 在清单 ✓（与撤回一致），`sql_update_record`/`l_sql_update_record` 随另一会话的**源码级删除**从清单域消失——其台账三处（§3.1:286、§2.1:112、§5.2:520）已无对应函数，**注销归那条删 bind 线收口，本阶段不代改**

### 阶段 6B — `query_apply` 删除（原 B2 补演示，改判）

> **改判（2026-11-09）**：复核标记曾判「动机与形态成立」，但按 6A 撤回同把尺再查一遍，
> 立不住——三棵 Lua 树（`lua-sample`/`lua-default`/`jianghu-dataset` 5316 文件）零调用点，
> C++ 侧除绑定自身零生产调用方；验看报数的需求已被五特化版（阶段 3 已演示）覆盖，
> 泛化版增量只剩「非五维属性也能读合计」，教材与数据集都无此需求。
> 数据集 10+ 处 `query("per")+query_temp("apply/per")` 手工组合全在 `per` 上，
> 是 `query_per()` 的等价语义（已演示），不构成泛化版用例。

- [x] 删实现 `CChar::query_apply`（`CharAttr.cpp`）+ 声明（`Char.h`）
- [x] 删绑定 `l_query_apply`（`CharAttr.cpp`）+ `LuaBindDecls.h` 声明 + `LuaBind.cpp` 注册表项
- [x] 删单测两处：`test_char_attr.cpp` 方法层、`test_bind_char_attr.cpp` 绑定层
- [x] §3.1 条目出列（`l_query_apply`）、§5.2 条目出列（`CChar::query_apply`）、B2 分档行改「已删（阶段 6）」
- [x] 验收同阶段 3（`run-coverage.ps1` delta ≥ 0 → `run-cppcheck.ps1` → `run-integration-tests.ps1` → `--check-lua` 三树）
- [x] 运行记录：2026-11-09 — ① `style.ps1` 跑过；② `run-coverage.ps1` 全绿：engine **2244/2244**（前轮 2246 − 删 2 用例）+ cli 280/280，Overall **94.7%**，**delta +0.0%**；③ `run-cppcheck.ps1` **0 问题**（123 文件）；④ `run-integration-tests.ps1` **PASS**：10 道 + persist 2 段 + reload 全过，未登记 ERROR 0（白名单 22）、崩溃 0，**215s / 420s 余量 +205s**（Δ −5s 未超参考阈值），关键路径 L10 213s、次长 205s，**零 lane 改动**（本阶段只删代码与用例，剧本/脚本无涉）；覆盖率 src **71.5%（11159/15598）**，未命中清单 **query_apply 两条已出列**（本轮改动新增未命中 0——纯删除不产生新函数；清单总数 204 与前轮 203 的轮间差属另一会话在途 `sql_update_record` 删除线的分母漂移，`sql_update_record` 本身已从 `src` 与清单同时消失）；⑤ `--check-lua`：`lua-sample` **60 OK / 0 FAILED**、`lua-default` **48 OK / 0 FAILED**、数据集 **5216 OK / 0 FAILED**（`bin\jianghu.exe` 仍被 6600 功能测试服锁住，沿 6A 先例用 `bin\e2e\jianghu.exe`、cwd=bin + `..\data\config.ini`；残留引用 0）

### 阶段 6C — 传送读条：`disable_player` / `enable_player`（0.5 天）

> **复核标记（2026-11-09）**：动机与落点**成立**——读条是给传送这个既有动作补它本来就该有的阶段（不是塞进别的交互），消费方是玩家体验。**四条执行约束（先解再写）**：
> ① **`call_out` 只有两个 int32 参数**（`l_call_out` 参数 4/5，`LuaBindGlue.cpp:170`），目的地是字符串 → 必须先 `set_temp` 暂存到玩家（或用下标），回调里再取出；
> ② `teleport_to` 是 **local** 函数、按名回调解析不到（本文件头注已写明脚本自定义函数不在 userdata metatable）→ 要么传**函数值**（绑定支持，`LuaBindGlue.cpp:188` 走 registry），要么改脚本表函数走蓝本字符串路线（owner=me 时回退 active script，`LuaBindGlue.cpp:179`——计划所称「与蓝本同路」属实 ✓，但 newyear 的回调函数 `do_zhuangbei` 是**脚本表函数**、不是 local）；
> ③ 读条时长必须 **< `poison_and_cure.sce:119` `wait room` 的超时**（该行未写 timeout、取 `-StepTimeout` 默认）；回调内 `move` 先、`enable_player` 后同拍完成，`wait room` 通过即已 enable，其后 `.info`/`/t 0` 不会被拒 ✓；
> ④ 「读条期命令被拒」建议**由「可选」升为必做**——否则 `disable_player` 只被「读条文案出现」证明，效果无断言；被拒文案按 §8.1 先手工交互取回。
> ⑤ 次要：回调时道具引用（回调 self = call_out 的 owner 即玩家）——扣 `uses`/`destruct` 需按 `base_name` 遍历玩家清单取回道具，蓝本同法（`item/cloth/newyear.lua:74`）。

- [ ] `item/sample_teleport.lua` 的 `teleport_to` 改三步（蓝本 `newyear.lua`）：`disable_player("传送读条中")` → `call_out` 延时 → 回调内 `move` + `enable_player`（回调函数解析走 active script，与蓝本同路）
- [ ] `social/poison_and_cure.sce` 传送段加读条 `wait text`；可选加「读条期命令被拒」断言（复用醉酒段已验证的禁用挡法）；**读条秒数别超该段后续等待窗口**（L4 时序风险，传送全仓真实使用只有这一处）
- [ ] §3.1 勾 2 条（`disable_player` 226、`enable_player` 227）
- [ ] 验收同阶段 3
- [ ] 运行记录：

### 阶段 6D — 房间动态门簇：拓展副本演示房（1~1.5 天）

> **复核标记（2026-11-09，按 6A 同一把尺自查）**：动机**成立**——动态门 / 巡逻 / 补货 / 战后回调都是房间本来就有的玩法，且 sample 已有 `arena`/`woods` 这类为机制而设的演示房先例；`yewai_grid.lua` 迷你版给随机门提供落点房间池（随机支的取名来源），不是可选项。**四处风险 + 两处台账错误，开工先解**：
> ① **两支同房会互相踩（计划内部矛盾）**：确定性支「talk 改门向」会被随机支的周期刷新改回——蓝本 `room/yewai_grid.lua:210/233` 的 `yewai_door_refresh` 是**自我续排**的 `call_out`，谁先起谁持续改门 → 必须**分房或分时段**（先断确定性支、停掉刷新再启随机支），计划未定；
> ② `call_out_combat`（type=1 等战斗结束才触发）要先定**确定能结束的战斗**：对手是谁、靠打死还是收手结束（阶段 4 有过「收手抢在死亡之前」的 `yield` 闸先例），否则 `wait text` 必超时；
> ③ `make_random_yewai` 迷你版一坑：房间池必须用**房间注册名**而非脚本名，否则 `load_room`/`bind` 拿到另一个实例——同 `sample_guide.lua:64` 注释记过的坑（用「青云山山顶」不能用 `sample_mountain_top`）；
> ④ **台账章节号写错**：本段写「§2.1 盲区②」——盲区② 在 **§2.2**（`docs\e2e-unhit-audit.md:118`），`l_call_out_combat` 的盲区② 条目在 **§2.2:129**，勾 §3.1:295 时须同步勾它（同 6A 的 DB 四条）；
> ⑤ **§3.1 行号全部漂 2 行**（计划写 297/306/323/325/326/327，实际 295/304/321/323/324/325；且 `bind` 对应符号是 `l_door_bind` `[bind]`）——执行时按符号名找（计划书首行已有此约定）；
> ⑥ 落道：片段体量大且含战斗 → 按当轮三行硬指标实测选道，**避开 L5/L6/L7 配对道**（§8.6⑦ clear 窗口），L1/L4 已有 6A 内容、富余需一并复核。

- [ ] 新增 `room/yewai_grid.lua` **迷你版**（`make_random_yewai(n)` 从演示区房间池随机取名）——**必须做**：随机支的门落点靠它取名，样例树缺此模块则随机支无从取池
- [ ] 新增演示房**放 L3/L8 握手链之外**（不复用两道链上房间，防并行互踩）
- [ ] 动态门两支：确定性支＝talk 改门向 → `PresentDoor(id)` + `door:bind(指定房)` → 过门 `wait room`；随机支＝`call_out` 周期刷新（蓝本 `yewai_door_refresh`：`room_exits()` 遍历 → `PresentDoor` → `bind(make_random_yewai(n))`）→ 断「过门落点 ∈ 房间池」（or 串），**不断「两次不同」**（随机可重复）
- [ ] `random_move` 巡逻：演示 NPC 置单出口演示房、talk 触发一次 → 过房 `wait object`（单门保证确定性）
- [ ] `add_item`：演示房触发补货 → 房内取走 `wait item`
- [ ] `call_out_combat`：演示房开战时挂战后回调（蓝本 `xunshou_shu` 战后支）→ kill 结束 `wait text`（type=1 回调等战斗结束才触发）
- [ ] 新片段挂非关键路径道、lane 只加 include 行（§8.7；落道以当轮三行硬指标为准，现关键路径 L3、次长 L8）
- [ ] §3.1 勾 6 条（`call_out_combat` 297、`random_move` 306、`bind` 323、`add_item` 325、`PresentDoor` 326、`room_exits` 327）+ §2.1 盲区②、§5.2 联动出列
- [ ] 验收同阶段 3
- [ ] 运行记录：

## 阶段 7 — 收口（0.5 天）

- [ ] 重扫 126 − 9（删）= **117** 条，核对清单收敛到「A 档剩余待演示」
- [ ] `docs\e2e-unhit-audit.md` §3.1 全量勾选/注销，标注各阶段归属
- [ ] `.\run-coverage.ps1` + `.\run-integration-tests.ps1` 终轮全绿，证据归档
- [ ] 删除本计划书（批次收口即删，不留长期文档）
- [ ] 运行记录：

---

## 全局纪律（每阶段都过一遍）

- 生产/教材注释只写机制、契约、边界；禁止出现测试计划、覆盖率、剧本、验收清单、文档编号作为改动依据
- `src/` 改动必配单测：`.\run-coverage.ps1` 是唯一认可的运行方式（Release，禁 Debug）；`run-cppcheck.ps1` 必过
- sample 是教材不是测试夹具：每个新调用先答「教材读者学它干什么」，答不出就不加
- E2E 改动过指令文件 §8.1~§8.7；新增预期服务端 ERROR 登记 `tests\integration\log-allowlist.txt`
- **改动绑定面（增删 `l_*` / 注册表项）后，必须跑以 `C:\jianghu-dataset` 为目标的 `--check-lua`**——数据集是绑定契约的下游消费方，它绿了才说明删除/新增没打破内容侧；两个目标（教材 / 数据集）的调用姿势与 `require(...) cannot resolve` 排查见 `copilot-instructions.md` 的 `--check-lua` 节
- git 只读（status / diff / show）

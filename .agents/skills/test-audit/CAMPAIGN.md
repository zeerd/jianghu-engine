# Test-pruning campaign（测试修剪战役）

战役模式在一次变更中修剪一个子系统拥有的全部测试面：例如 LuaBridge 绑定层（`test_bind_*`）、channel 子系统、或 cmd 指令族。[SKILL.md](SKILL.md) 的价值标准、保留标准、候选证据与验证适用于每个车道；本文件补充完整战役的工作顺序与经验。每一步以完成标准为界，不要提前开始下一步。

## 1. Baseline（基线）

在固定 SHA（`git rev-parse HEAD`，只读）处记录：子系统的测试与支持行数（`tests/*.cpp` 与 `mock/`，生产代码单独计）、每个测试文件的通过/失败状态、一次全量 `.\run-coverage.ps1` 的测试总数与 `coverage_history/summary.csv` 末行。基线失败单列成清单 —— 历史战役中基线失败全部是真实产品 bug，不是陈旧测试。

完成标准：每个在范围内的测试文件都有记录的基线结果。

## 2. Lanes and inventory（车道与清单）

按**生产 owner 边界**切分车道，不按文件前缀。本项目的默认车道（起点，第 3 步的证据可纠正归属）：

1. **bind** — `test_bind_*` ↔ `src/lua/`（LuaBind、LuaBridge 注册、dispatcher）
2. **char** — `test_char_*` ↔ `src/game/`（CChar 属性/战斗/死亡/存档）
3. **cmd** — `test_cmd_standard_*`、`test_cmd_user_*`、`test_cmd_wizard_*` ↔ `src/cmd/`
4. **container+item** — `test_container_*`、`test_carry_object`、`test_stacking`、`test_setup_objects` ↔ `src/core/`、`src/item/`
5. **lua 引擎与脚本** — `test_lua_*`、`test_db_layer` 中的引擎侧用例 ↔ `src/lua/`（CLuaEngine、脚本隔离、hook 回退）
6. **db** — `test_db_layer`、`test_sql_cards`、`test_loginregister`、`test_checkplayer`、`test_gate_info` ↔ db 层 + `mock_db`
7. **channel** — `test_channel_subsystems`、`test_channel_deep` ↔ `src/engine/`（Snoop/Ban/Vote/Emote + facade）；`test_container_ownership.cpp` 中的 snoop 双份用例位于边界，归属由第 4 步 keeper 裁决
8. **combat** — `test_combatcalc`、`test_combatconfig`、`test_weapon_effect`、`test_fightroom`、`test_fb_registry`、`test_force_adv_recover` ↔ `src/skill/`
9. **util 基础库** — `test_stringutil`、`test_mapping`、`test_queue`、`test_random`、`test_log_levels`、`test_encoding`、`test_env` ↔ `src/util/` 与核心容器
10. **harness** — `test_smoke`、`mock/*`、`test_config.ini`、公共头 `test_*_common.h`，以及测试支撑脚本

未列出的文件按其生产 owner 归入上述车道；含共享核心边界的用例归契约 owner 所在车道。harness 车道同时覆盖 QA 与自证类测试。

完成标准：每个测试文件恰好属于一条车道。

## 3. Read-only ledger per lane（逐车道只读台账）

每条车道交给独立的只读 agent。它完整读取全部被分配的测试（包括参数化表），也读生产 owner 及其入口、调用方、历史（`git log` 只读）与测试注册路由（`tests.vcxproj` 的 `<ClCompile>` 条目、`run-coverage.ps1` 的收集范围）。每条测试声明进一份书面**台账**，打一个标记。Google Test 的参数化用例（`TEST_P`、循环生成的多行）算一条声明，除非各行需要不同标记；需要时逐行标。

- `R`：保留，写明它守护的契约与能抓的 bug；仅挪到更准确命名的文件仍算 `R`，注明移动；
- `F`：保留契约但修复断言，例如"多项缺一仍通过"的空洞负断言；
- `C`：合并，写明首先吸收该断言的 owner：兄弟表驱动用例、更强的边界套件，或另一处的共享 owner；
- `D`：删除，写明剩余的证明，或为何不存在契约。

按断言判定测试，不按名字。曾有测试名为"清理窗口"，断言却是窗口**没**被清理。

完成标准：车道内每条声明都有标记和证据行。

## 4. Layer plan per lane（逐车道分层计划）

逐测试台账是输入，不是编辑清单。第二遍只读从台账出发寻找冗余的**层**。本项目的常见形态：同一契约在多处重放 —— 手写重实现的 stub、测试内复制的生产逻辑、纯 mock 状态断言 —— 绕着更强的"真实 `jianghu_core.lib` + mock 链接层"套件。为每个契约点名 **keeper**（保留套件）。偏好真实边界 + 假外部依赖（`tests/mock/` 链接时替换 mock_net/mock_db/mock_game），优于 mock 掉协作者或在测试里复刻实现。这一遍同时纠正台账错误。

完成标准：每条车道计划写明退役文件、每契约的 keeper、要搬进 keeper 的断言、以及解锁的测试专用生产 seam。

## 5. Cutover（切换）

逐车道编辑。共享 harness（`mock/`、`test_config.ini`、公共头）的修改串行通过单一 owner，避免并行冲突。每条车道顺手删除它解锁的测试专用生产 seam：注入参数、getter、reset 导出、中间层 —— 判据见 `.agents/instructions/unit-test.instructions.md` 的"可测试性设计：自由函数无法 Mock，必须类化"：生产调用方也需要的接口是设计，只有测试需要的才是 seam，直接删。移动的套件在 `tests.vcxproj` 的 `<ClCompile>` 中注册/注销；覆盖率基线按 shrink-only 更新（delta ≥ 0）。把本战役实际发现的持久测试归属规则写进 `.agents/instructions/unit-test.instructions.md`。

完成标准：每条车道计划已应用，该车道 keeper 全绿（聚焦运行；车道全部完成后跑一次全量 `run-coverage.ps1`）。

## 6. Preservation review（保全评审）

声称完成前，让独立评审按边界组比对被删覆盖与 keeper。他们找两类问题：失去唯一证明的契约；以及不可能失败的新断言，例如生产代码永远走不到的拒绝行。历史战役的一次评审找出九个真实缺口和一个不可达断言。

对每个恢复的契约，对生产 owner 做一次故意**变异**，确认 keeper 变红，然后逐字节恢复源码。变异验证用聚焦运行；车道全部完成后跑一次全量确认。

完成标准：每个报告的缺口要么恢复（附捕获变异的证明），要么凭源码证据驳回。

## 7. Product defects（产品缺陷）

存活进 keeper 的基线失败就是 bug 报告。在 owner 处作为独立变更修复，并通过真实用户流程证明：**control** 运行还原修复、展示旧的错误行为；**candidate** 运行展示修复后通过 —— 同一 harness 上两者都要有。顺带发现的无关产品缺陷记为后续项，不在战役里修。

完成标准：每个修复的缺陷都有失败的 control 与通过的 candidate。

## 8. Reconcile and hand off（对账与交接）

战役会跨越许多 `main` 提交。**合并 `main` 由用户执行**（agent 仅允许只读 git），不做长时间多提交的 rebase。`main` 改动了战役已删除的文件时，保持删除，把新契约移植进 keeper，并确认 `main` 新增的每个回归都有归属车道。在合并后的头上重跑全量 `run-coverage.ps1`；涉及 Lua/绑定的改动重跑 `--check-lua` 全量。

如此大的 diff 会让评审工具只看到截断的文件清单。维护者对通用兼容标志的决定记录进交接报告，而不是去改门槛。

交接时按 [SKILL.md](SKILL.md) 的 Handoff 报告，外加：

- 基线与最终的测试/支持行数（生产代码单独计）；
- 车道、退役层与 keeper；
- 保全缺口及其变异证明；
- 产品缺陷的 control/candidate 证明。
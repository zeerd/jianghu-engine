---
name: test-audit
description: "每当编写、修改、评审或清扫测试时调用：新测试的编写门槛（authoring gate），以及针对低价值、实现耦合、重复测试与其维系的测试专用生产 seam 的审计工作流与子系统战役流程。适用于本项目 Google Test（tests）的 C++/Lua 测试面。"
---

# Test Audit（测试审计）

三种模式，一条价值标准。编写模式在写入时把关每一个新增或修改的测试；审计模式针对重断言源码、重复更强证明、耦合实现细节、或为测试专用生产 seam 续命的测试做定点清扫；战役模式一次修剪一个子系统拥有的全部测试面，开始前先读 [CAMPAIGN.md](CAMPAIGN.md)。广泛审计按主题拆成独立的后续变更；优化目标是置信度，不是删除数量。

本项目测试面：`tests/` 下 Google Test 的 `test_*.cpp`（唯一测试工程 `tests.vcxproj`），链接 `jianghu_core.lib` 真实实现，外部依赖由 `tests/mock/` 链接时替换。测试架构、覆盖率门槛与运行方式以 `.agents/instructions/unit-test.instructions.md` 为准，本文件是其评审与修剪配套。

## Authoring gate（编写门槛）

新增任何测试前先回答四个问题；缺一个答案就先别写：

1. 它保护什么可观察行为、不变量或独立契约？
2. 什么可信的回归会让它失败？
3. 为什么现有覆盖抓不住那个失败？每个契约在最强边界只有一个主测试 owner；另一层要有自己独立的风险（如真实传输层或生命周期失败，owner 够不到）。优先扩表驱动用例或共享 fixture，而不是近重复测试；重复的 setup 在同一次改动中合并。
4. 它需要一个没有生产调用方的生产 seam（导出、标志、包装、注入钩子）吗？需要就把测试挪到真实边界去。

然后对照下文 **Junk patterns（垃圾模式）** 清单；命中即不通过，除非 **Retention bar（保留标准）** 点名了它独立守护的契约。在保持行为的重构下会碎的测试，断言的是实现而不是行为；先在 owning boundary 重写再落地。

Bug 回归测试必须在修复前的代码上因预期原因失败、在 owner 边界修复后通过。从未真正失败过的回归测试证明的是 mock，不是修复。一个 owner 边界的回归就够覆盖该 bug；不要在它跨过的每一层重放同一场景。

本项目强制规则（来自 unit-test.instructions.md，优先于本文件的宽松解释）：

- 修改 `.cpp`/`.h` 必须带测试提交；
- 验证以仓库根目录 `.\run-coverage.ps1` 为唯一认可的编译运行途径，`coverage_history/summary.csv` 的 `delta` 列必须 ≥ 0。

## Junk patterns（垃圾模式）

两种模式共用的清单：编写门槛拒绝命中的新测试，审计则搜寻命中的存量测试。

通用：

- 无断言的覆盖率探针；
- 自比较与恒等拷贝；
- 复制的 fixture、清单、账目或导出列表；
- 对源码、导入或字符串的精确 grep；
- 在真实边界重复出现的私有谓词或调用形状测试；
- 同一契约的重复调用；
- 在提供方本地重放共享 helper；
- 唯一目的是保住测试专用导出、全局量或包装的测试；
- 唯一调用方是测试的死生产代码；
- 期望值由被测 helper 或渲染器自身算出；
- mock 自己实现了被断言的行为，或同一个 mock 冒充不同 API；
- fixture 代产了 owner 应当产生的回执、准入或回调顺序，或断言的持久化路径根本不写那个存储；
- 复述已声明标志的能力测试，而不是演练标志承诺的投递或确认；
- 因无关原因通过的负控制：来自另一个守卫的拒绝，或生产路径永远到不了的拒绝；
- 名字或 fixture 承诺多于输入实际演练的内容（名为"清理窗口"的测试断言窗口没被清）。

本项目已知实例（审计时优先猎取）。**发现即处置**：当场修断言、合并或删除，或登记进战役台账与车道：

- `EXPECT_TRUE(true)` / `SUCCEED()` 占位断言与为凑覆盖率 delta 而写的探针 —— 现查 `grep -E "EXPECT_TRUE\(true\)|SUCCEED\(\)" tests/*.cpp`，注释命中豁免；
- 在测试或 stub 里复制一份生产实现再断言它（历史教训：`test_combatd` 对 `EffStatusMsg` 的断言在 stub 中复制了实现，已删）；
- 只断言 mock 自身状态（CallRecord、ok 之类标志）而生产侧没有对应调用契约的测试；测试之间不 reset 导致的顺序依赖测试；
- 同一契约的双份测试 —— 对照现查（`Select-String "TEST\("` 两侧计数，同有即在案）：`test_channel_subsystems.cpp` 的 `SnoopSystemDirect` ↔ `test_container_ownership.cpp` 的 `ChannelSnoopList`；`test_smoke.cpp` 的 `MappingBasic`/`StackingLogic` ↔ `test_mapping.cpp`/`test_stacking.cpp`；
- 仅为测试存在的生产 seam：没有生产调用方的接口 + 全局指针、getter、reset 导出、注入参数。判据见 `.agents/instructions/unit-test.instructions.md` 的"可测试性设计：自由函数无法 Mock，必须类化"：生产调用方也需要的接口是设计，只有测试需要的才是 seam，直接删。

## Value bar（价值标准）

测试靠保护行为、可信回归或独立有意义的契约来证明维护成本。审计中，一个"为保持行为的源码重组就必须改"的测试是可疑对象，但不自动可删；编写门槛仍然拒绝新增此类测试。

判定候选前，读完整的测试与生产 owner、它的入口、调用方、被调方、兄弟实现、重叠测试、测试注册路由（`tests.vcxproj` 的 `<ClCompile>` 与 `run-coverage.ps1` 范围）与相关历史。**先读根 `AGENTS.md` 与 `.agents/instructions/*.instructions.md`**（copilot-instructions、unit-test 等）。测试声称依赖某库行为时，直接读依赖源码或类型（如 LuaBridge）。

## Discovery（发现）

发现阶段保持只读，先报告证据再编辑。范围大时按生产 owner 并行开道：

- 绑定层（`test_bind_*` ↔ `src/lua/`）；
- 角色与指令（`test_char_*`、`test_cmd_*` ↔ `src/game/`、`src/cmd/`）；
- 容器/物品、db、channel/combat、util 基础库；
- harness（`tests/mock/`、`test_config.ini`、公共头、`test_smoke`）；
- 横切扫描：按 Junk patterns 清单关键字全量检索。

非战役模式下，宁要少量高置信候选，不要大量投机清单。

## Retention bar（保留标准）

当测试独立守护以下契约时保留：LuaBridge/C 绑定注册（绑定丢失是真实历史风险，`object_methods[]` 与 `beginClass<T>` 注册表是唯一 C→Lua 入口）；Lua hook 回退与脚本隔离；mock 链接契约（LNK2005、RTL/CRT 匹配）；配置与 `test_config.ini` 路径契约；Save/Load 存档数据格式；GBK/UTF-8 编码转换；命令入口（Handlecommand 分发）；崩溃回归（`test-crash-patterns` 教训）——但“存活即通过”必须转成可观察断言：前后对照计数（如析构前快照 `g_call_count`）、空状态返回 0（空队列 `handle_command()==0`）、对照组无副作用（`is_fighting()` 恒 false）；实在没有可观察面时，注释必须写明所守护的存活契约与失败方式（崩溃即测试失败），否则按探针处理。另保留：

- 顺序本身是可观察行为的调用顺序；
- 有可信失败模式的回归；
- 源码检查作为最廉价的独立守护：契约（用户可见的键、字节、路径）变了就失败、纯改名不碎 —— 本项目实例是 grep `object_methods[]` 判定 `self:xxx()` 是否已绑定；
- 在基线上就失败的保留测试：当作疑似产品 bug，复现并在 owner 处修复，而不是删除。

慢、静态、覆盖率低都不是删除理由。长得像实现的测试可能恰恰是独立契约；先证明它不是再删。

## Candidate evidence（候选证据）

编辑前记录全部字段，缺一项就不具备删除资格：

- 测试的精确名称与位置；
- 它实际能检测到什么失败；
- 被覆盖的生产或支持 seam 的非测试调用方；
- 更强的剩余 owner 边界证明，或为什么不需要证明；
- 相关历史，以及该测试或 seam 为何存在；
- 可解锁的生产或测试支持删除（含 `tests.vcxproj` 条目）；
- 风险与聚焦验证命令。

## Edit shape（编辑形状）

选一个内聚的 owner 边界批次动手。删除过时的测试专用导出、全局量、包装和死生产路径，不要留兼容别名。保留的回归挪到其规范 owner。重复的依赖断言合并为一条通用契约。

优先净负的生产 LOC。不要补一个复述同一实现的替代测试，也不要把不确定的候选转成清理来刷删除数。本项目处于研发阶段：结构不合理立刻调整，不留向后兼容、不为"最小变更"回退。

## Validation（验证）

`run-coverage.ps1` 运行期间不要编辑源码或测试。禁止手动 `msbuild tests.vcxproj` 后**在非仓库根目录**直接运行 `tests.exe` —— 工作目录不在仓库根目录会让 `test_config.ini` 与 Lua 相对路径全部失效（聚焦调试与变异验证的受控例外见下文第 9 条；权威表述在 unit-test.instructions.md）。

1. 聚焦运行：构建由 `.\run-coverage.ps1` 完成后，在**仓库根目录**执行 `.\tests\Debug\tests.exe --gtest_filter=<Suite>.*`，跑最小 owner 与兄弟套件。
2. 对被删的源码 grep 或计划断言，跑拥有真实契约的可执行验证（如 `--check-lua` 全量、干跑脚本）。
3. 格式化与洁净：C++ 跑 `.\style.ps1`，Lua 改动跑 stylua（命令见 copilot-instructions.md），然后 `git diff --check`。
4. 全量验收：`.\run-coverage.ps1`，确认 `coverage_history/summary.csv` 的 `delta` ≥ 0；涉及 Lua 的改动追加 `ccjh.exe --check-lua` 全量（禁止抽样，调用方式见 copilot-instructions.md）。
5. 静态检查按需：`.\run-cppcheck.ps1`。
6. `git diff --numstat` 分列生产/工具与测试、测试支持；并自查 copilot-instructions 硬规则：无迁移痕迹注释、无 BOM、仅 Win32。
7. **新断言失败先归因**：新写/重写的断言变红时，先读被测生产语义判定归属 —— 期望写错则修期望（典型如：`analyse_string` 是空格分隔不是 `=`；无 hard-core 标记的技能 push `nil` 而非表；`move()` 拒收 `amount>1` 的非叠加物品；`lua_check*`/`analyse_string` 对错参数会抛错或写穿只读内存而调用包装会吞掉异常）；只有复现确认是产品缺陷才走下文 Product defects 修生产。**禁止改生产代码迎合新断言，也禁止删断言保绿**。
8. **编辑落地核对**：批量/多块编辑工具可能部分失败却返回成功摘要 —— 每个计划改动都要 grep 逐条确认已落地，并用 `git diff --numstat` 复核文件清单与预期一致。
9. **变异验证的编译规程**（配合 CAMPAIGN 第 6 步）：变异后手动 msbuild 编译，在**仓库根**以 `--gtest_filter` 聚焦运行；变异结束逐字节恢复（`git diff` 为空）并重编译确认；变异状态下禁止跑 `run-coverage.ps1`（污染 `coverage_history/summary.csv`）。

## Landing and continuation（落地与继续）

本项目 agent 只允许只读 git（status/diff/show）；提交、合并、推送一律由用户执行。一次落地一个内聚变更；落地后从当前 `main` 刷新，重新做只读发现，挑选下一批高置信候选。

## Handoff（交接）

报告：

- 根因与删除的低价值类别；
- 生产 owner 的简化（净负生产 LOC）；
- 保留的误报及保留理由；
- 实际运行过的聚焦与全量证明（含覆盖率 delta 前后值）；
- 生产 vs 测试 LOC（`git diff --numstat`）；
- 变更与提交状态（提交动作由用户执行）；
- 点名的后续项。
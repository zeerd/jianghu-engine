# docs（plans 除外）残留「已知问题 / 待确认 / 存疑」描述核查与修订计划

> 核查基准：2026-11-12，对照当前工作区代码。
> 每条先给「核查结论」（仍有效 / 已失效 / 部分过时），再给动作。证据行号为核查时快照，实施时以符号名直查。
> 范围排除：E2E 未命中清单/认领/台账等集测域材料随轮次变动，不在本计划内（归 `docs/design/integration-test*.md` 域）。

## 批次 C：行号与文件引用漂移（误导读者按图索骥）

> 本批次只按「文件 + 函数/符号」定位，不登记任何精确定位行号（行号随代码漂移，登记即过期）。

- [x] **C1 `docs/handle_action.md` 行号批量漂移（仍有效）**
  - 核查：文档内约 11 处以行号锚定的引用已漂移，涉及
    `CharInterface.cpp`（`CChar::handle_action` 三参/四参）、`Room.cpp`（`CRoom::handle_action` 四参）、
    `CmdHandle.cpp`（`CCmdHandle::command` 及其环境/目标拦截分发）、`CmdStandardCombat.cpp`（`CCmdStandard::guard`）、
    `LuaRoom.inl` / `LuaNpc.inl` / `LuaItem.inl`（各 `handle_action` 转发）、
    `LuaBind.cpp` 与 `LuaBindGlue.cpp`（`base_handle_action` / `base_handle_action4` 绑定与实现）、
    `SqlRank.cpp`（榜单 `handle_action` 调用）。
  - 动作：全量改为按符号直查定位（或去掉行号只留文件+函数）；机制描述本身未发现失实，不改文字。
  - 验收：抽查 10 处锚点全部命中所述函数。
- [x] **C2 `docs/skill_magic_mechanism.md` 悬空文件与行号（仍有效，含 3 处死路径）**
  - 核查：3 处引用 `src/lua/LuaSkill.inl`——**该文件不存在**，实现已并入 `LuaSkill.cpp`
    （`CLuaSkill::setup_base_skill`、`CLuaSkill::inherit_base_magics`、施法分派）；
    另有若干以行号锚定的引用已漂移，涉及 `Skill.cpp`（`l_add_magic`、`DOWNMAGIC` 下发）、
    `CharSkill.cpp`（`skill_map`）、`SkillMgr.cpp`（`g_skill_list` 单例）、
    `CmdStandardCombat.cpp`（`CCmdStandard::cast` 解析顺序）、`LuaEngine.cpp`（热重载连带 `skill_release_one`）、
    `Skill.h`（`has_hit_ob` / `has_query_action`）；`CSkill::cast`、`CSkill::inherit_magics` 两处仍准。
  - 动作：死路径改 `LuaSkill.cpp` 对应符号，行号全量改为文件+函数定位。
  - 验收：全文件不再出现 `LuaSkill.inl`；抽查锚点命中。
- [x] **C3 `docs/design/gdd/combat-system.md` 行号 + 两处代码缺陷（含代码修复）**
  - 核查：两处引用 `Attack.cpp` 中 `unsigned long my_exp` 的 `< 0` 死分支——行号已漂且死分支属实；
    `CReward.cpp` `handle_pk_reward` 的 `gain = 0` 表达力问题属实，且连带三重死码
    （复仇分支用指针比较、无条件清零使整段失效、恒假 clamp）与调用方死 gain 计算链。
  - 动作：文档定位方式改文件+函数（已落地）；**代码缺陷一并修复**——删 `Attack.cpp` 死分支
    （设计伪代码本无此钳制，删除为零行为变化）；`handle_pk_reward` 移除 `gain` 参数与
    复仇死分支/死 clamp/`kill/last_lose` 死状态，`gain = 0` 常量 + 设计注释显式化
    （设计：PK 击杀者经验恒 0，作者裁定）。
  - 验收：`run-coverage.ps1` 全绿、delta ≥ 0；`run-cppcheck.ps1` 零问题；测试断言
    `kill/last_killer` 仍写入、`kill/last_lose` 不再存在、`combat_exp == 0`（设计语义）；
    `combat-system.md` §4/§5/§8 各项已同步勾销。
- [x] **C4 `docs/call_out.md` 示例文件路径为虚构（部分有效）**
  - 核查：两例标注 `npc/sample_healer.lua`、`item/sample_amulet.lua`，三仓（lua-sample/lua-default/dataset）均无此文件。
    示例代码本身自洽，属教学示意。
  - 动作：路径改为「示意文件名」措辞（如 `-- 示意：<你的 NPC 脚本>.lua`）。
  - 验收：读者按文档找不到文件时不会认为文档失实。

## 批次 D：`engine-core-architecture.md` Known Issues / Open Questions 复核

- [x] **D1 逐条复核与修订（除 x64 外全部落地；x64 项有意保留不动）**
  - `SetEvent(g_event)` 指向从未 `CreateEvent` 的句柄：**已修**——`g_event` 定义、
    `SetEvent` 调用与 extern 声明一并删除（无人等待该事件，关服握手走 worker 线程句柄）。
  - `db_save_box` 族 + `select_game_database` 仅测试 stub：**已修**——Sql.h 声明与
    mock 实现删除，`unit-test.md` mock 表同步。
  - `t_saveobj`、`user` 表 DDL 无调用方：**已修**——DbTables DDL 删除（`AutoPkColumn`
    仍服务于 jianghudb），测试侧表清单同步、6 条 t_saveobj 重复测试删除/改定向。
  - `g_command_handler` 疑似死代码：**确认为死并已清理**——定义与 extern 删除，
    实际使用的 `Char.cpp` 的 `CommandHandle` 不动；文档 Known Issues/Open Q3 随之移除。
  - `g_gate_list` 数据竞争：**已修**——`push_back` 与 erase 循环补 `g_gate_list_mutex`，
    `empty()` 检查移入锁内；Follow-Up「Immediate」已勾。
  - `on_login` 持 `g_gate_list_mutex`+`m_cs` 做 DB：**已修**——LOGIN 分支先在 `m_cs`
    内核状态、再把两把锁都释放后调 `on_login`（gate 由列表引用保活，摘除只在收尾段）；
    Follow-Up 对应项已勾（MySQL 语句超时仍 open，单独成条）。
  - `load_script()` 缓存查找前整文件哈希：**已修**——命中先比文件指纹
    （mtime+字节数）不读盘，指纹变化才读盘算哈希确认（与 `check_for_reload`
    两段快筛同口径），未命中才读盘；Follow-Up 已勾。
  - x64 风险（3rd/lib 仅 x86 库、bin 输出目录共用）：仍成立（README:33 仍明示仅 Win32），
    **有意保留**——属 D1 排除项，不随本批次处理。
- [x] **D2 Open Question 1「安全姿态」事实过时（部分过时，已由 A1/A2 落地）**
  - 核查：「玩家密码明文存于 `cc_passwd`」已过时——口令已改 PBKDF2 加盐哈希 + 读时迁移
    （encryption.md §2，`make_password_credential`，SqlAccount.cpp:111 等）；仍成立的子项：
    DB 口令明文在 config.ini（root/root）、默认巫师密码 12345678（Sql.cpp:101）、
    暗码旧路径 `OLD_PASSWORD`（SqlAccount.cpp:99）。
  - 动作：改写该 Open Question 的「代码事实」段（密码部分指向 encryption.md），其余子项保留；
    「是否接受」的裁定仍留待作者，不代答。**文档已改写，本批次复核通过。**
- [x] **D3 ADR 其余 Open Questions（文档已调整，随 D1 收口）**
  - Q2（Alternatives 未确认）保持原状待作者答复。
  - 原 Q3（gen_apply.py 去向）：文档条目已移除（`LuaBindDecls.h` 注释仍称由该脚本生成，
    注释失实随 bind 精简轮处置，不在本批次）。
  - 原 Q4（g_command_handler）：随 D1 确认为死码并清理，已从 Open Questions 移除。

## 批次 E：概念文档与 GDD 的待打磨项复核

- [ ] **E1 `docs/design/concepts/jianghu-engine.md` §5 三条待打磨（仍有效）**
  - `command/` 有壳无货：仍成立（`lua-default/command/` 仅 README，`lua-sample/command/` 不存在）。
  - `--check-lua` 缺 hooks 名字白名单：仍成立（LuaSyntaxChecker 只有绑定/方法白名单，无 `hooks.*` 校验）。
  - `gain = 0` 表达力：**已由 C3 销账**（2026-11-12 代码修复 + concepts §5 条目改 ✅）。
  - 动作：文档不动（属待办而非失实）；实施侧各自立项时销账。
- [ ] **E2 §13 开放问题与 §7 清单中的过时项（部分过时）**
  - Q6「编码统一的破坏半径」已失效——编码统一 2026-10-24 已完成（本文档 §4.1 自述），应删或改为「已解决」。
  - §5「注释漂移（§4）：三处已知」指代不明——§4 现只列两条已修复项，未枚举「三处」，需重查或删。
  - §7 未勾三项（`command/` 迁入、x64、分线）仍有效，保留。
- [ ] **E3 等级上限表述不一致（仍有效，轻微）**
  - 核查：game-brief:31 写「175 级经验表」，concepts:347 写「等级上限 176」；
    `level_exp.lua` 头注称 1..176 但表最大键 `[175]`，`NORMAL_MAX_LEVEL=176` 且 lv≥176 走 C++ 外推
    （CharLevel.cpp:147）。
  - 动作：统一口径（建议「175 级经验表 + 176 起外推」），顺带修正 `level_exp.lua` 头注与表实际键域的出入。
- [ ] **E4 hook 计数口径不一致（存疑，需实测）**
  - 核查：ADR 写「`hooks.*` 表共 25 处调用点」，concepts 写「30+ hook」，两文档同日反向生成，未复核。
  - 动作：数一遍 src 内真实调用点，两处改为同一数字（或改为「以代码为准」不再复述数字）。

## 批次 F：结构清理

- [ ] **F1 空目录 `docs/architecture/`、`docs/guide/`（0 个条目）**——删除，或写明用途后补内容（二选一）。
- [ ] **F2 `docs/design/game-brief.md` / `concepts` / `gdd` 的 `INTENT UNKNOWN`、`verified-by: 待作者确认` 标记**
  - 核查：属反向文档的有意标记，**不是残留缺陷**，不动；仅在作者逐项确认后按各文档自述流程摘除。

## 已核查、确认无需动作（留档防重复调查）

- `docs/design/encryption.md` §3「尚未做」：有意留待的取舍，非缺陷。
- `docs/design/unit-test.md` mock 表述：与 `mock_db.cpp` 实现一致，无失实。
- `docs/design/integration-test.md`：`--modules bin\e2e\jianghu.exe`、420s 预算、§6 域级表均与脚本/最新轮一致。
- `docs/design/integration-test-adjudications.md` 中 `lockbox`/`unlockbox`「未落地」：src 无该命令，仍准确。
- concepts §4 两条「已修复（2026-10-24）」、§3 各 ✅ 条目：与代码一致。
- `docs/design/gdd/combat-system.md` 的 `INTENT UNKNOWN` 与作者裁定（legacy/教学占位）：保持。

## 运行记录

- 2026-11-12：完成全量核查（13 份文档 vs 代码 + 11-12 轮 E2E 清单），落盘本计划；未改任何文档。
- 2026-11-12：A1/A2 落地——`protocol.md` §1.4 密钥改 15 字节 + 回绕改 15 + 补 `comm_key` 可配置说明；
  `engine-core-architecture.md` 两处「固定 9 字节密钥」改「默认 15 字节、可配置」，Open Question 1
  代码事实段同步（密码改指向 encryption.md、巫师密码行号改函数定位）。
- 2026-11-12：批次 C 文档改动已写入（四份文档行号删除、`LuaSkill.inl` 死路径改
  `LuaSkill.cpp`、`call_out.md` 示例路径改「示意」），但条目**保持未勾**——待逐条验收
  （抽查锚点命中、问题销账）后再勾。
- 2026-11-12：C1/C2 验收通过（抽查锚点全部命中所述符号）；C3/C4 文档侧复核亦已满足，
  随 C3 代码修复一并勾销。C3 代码修复落地：`Attack.cpp` 删 `my_exp < 0` 死分支；
  `CReward.cpp`/`CReward.h` 移除 `handle_pk_reward` 的 `gain` 参数、复仇死分支、
  恒假 clamp、`kill/last_lose` 死写入与 `killer_reward` 死 gain 计算，`gain = 0` +
  设计注释显式化；`test_reward.cpp` 同步（删死分支喂入、状态断言、行号注释清漂）。
  验收：2253+288 测试全绿，覆盖率 94.7% delta +0.0%，cppcheck 零问题，style 已跑；
  `combat-system.md`（§4 表达力条目、Handled 两条、DoD、Follow-Up）与
  `concepts/jianghu-engine.md` §5 `gain = 0` 条目同步销账；E1 第三条随之完成。
  注：期间工作区有并行会话在做 bind 精简（Door/bind 等），曾致一次构建失败与一次
  覆盖率采集污染（summary.csv 中 3367 行数的失真行），重跑后读数干净。
- 2026-11-13：批次 D 落地——D1 除 x64 外全改（删 `g_event`/`SetEvent`、`db_save_box` 族、
  `select_game_database`、`g_command_handler`、`t_saveobj`+`user` DDL；`g_gate_list` 补锁、
  `on_login` DB 移出列表锁与 `m_cs`、`load_script` 命中改文件指纹快筛）；D2/D3 勾销
  （安全姿态段 A1/A2 已改写、gen_apply 与 g_command_handler 条目文档已调整）。文档同步：
  Known Issues 整节移除、Consequences 两条重写、热点改指纹口径、Follow-Up 四项勾选、
  Open Q3 移除、unit-test.md mock 行同步。测试：新增锁契约（mock verify_user 观测点 +
  探测线程 try_lock）与快筛契约 3 例，先红后绿；纯 mtime 快筛首跑被 `ReloadFailures`
  同刻度写入判红，升级为 mtime+字节数指纹后收敛；删 6 条 t_saveobj 重复测试、表清单减 2，
  登录帧构造抽 `test_login_frame.h` 共享。验收：单测 2247+288 全绿、覆盖率 94.7%
  delta +0.0%、cppcheck 零问题、style 已跑；E2E 10 道+persist+reload 全 PASS
  （ERROR 14 全登记、224s/预算 420s、Δ+0s）。

# E2E 未覆盖项补测与多账号互动测试计划

> **基线**（2026-10-27 运行，`coverage_history/summary-e2e-sample.txt`）：
> src 行覆盖率 50.1%（8268/16501）；未命中函数 458/1163（C++ 300 + Lua 绑定 158）；
> 0 命中文件 23 个（最大：LuaSyntaxChecker 659、CmdWizardSystem 307、CmdUserParty 177、
> CmdWizardAdmin 171、NpcMirror 121、CmdWizardWorld 92）；最弱目录 cmd 25.5%、npc 28.3%。
>
> **进度跟踪**：每个条目前的 `- [ ]` 勾选即表示完成；阶段末尾有验收清单。
> **决策记录**：**不测巫师指令**（B 类全部排除，见 §2）。

---

## 1. 阶段 P1：单账号补漏（现有四道内加片段，低风险）

不改基础设施，只往现有 lane_*.sce 里塞片段。遵守 `.claude/rules/integration-test.instructions.md`
§8 的断言纪律与 `/t` 目标纪律。

### CmdStandardItem（drop / unwield）
- [x] `drop`：L1 扔一件物品，断言本人版「你向…丢下…」+ `object` 条件复核物品落进房间
- [x] `un_wield`：L1 `/unwield 铁剑`，断言卸下本人回执（模板 `$N将手中的$n插回剑鞘`；DSL 无反向 wield 条件，回执仅在 do_action 成功后才发）

### CmdStandardLife（wakeup2 / halt_exercise）
- [x] `halt_exercise`：L3 打坐中途 `/halt`，断言中断回执与 mp 停止增长
- [x] `wakeup1`（计划原误写为 wakeup2）：单人睡到自然醒——现有 `levelup_rest.sce`「你一觉醒来」已覆盖，无需新增
- [ ] `wakeup2`：**依赖双人同睡分支**（异性双方 IS_LIVING、sleep_room、hp≥50%，见 `CmdStandardLife.cpp` Sleep 后半段），单账号不可触发 → 移到 **P3 互动道**（A `/t B` + `/sleep` 同睡到自然醒）

### CmdUserAccount（账号类命令）
- [x] `save`：任一道中途显式 `/save`，断言存档回执（L4 account.sce）
- [x] `quit`：某一道最后一步 `/quit`，断言连接正常收尾（注意放片段最末）（**L1 lane_foot 末行** + wait battle_end 前置；见下方竞态修复条，quit 由 L4 尾部移至 L1）
- [x] `passwd` + `verify_pass` + `validate`：L4 改密码→验证→登录校验链（account.sce 含两段式改密 + 暗码短/成功 + 验证密码错/暗码错/通过 共 9 分支）
- [x] `tune`：L4 切频道，断言频道开关行为（关成功 + 再关落 else 分支）
- [x] `mudage`：L4 查询游戏年龄
- [x] `unset`：L4 清一个已 set 的参数（与已有 set_param 覆盖配对）
- [x] `refuse`：L4 屏蔽设置一条（无设置/关一条/列表/none 恢复 四分支）

### CmdUserSkill / CmdStandardMaster / CmdUserSocial
- [x] `skill`：L2 `/skill` 查看自身技能 —— 实现修正：回执是 RTNSKILL 协议（CLI 静默合并、无文本行），断言取「不出现权限拒绝/未知命令」证明走通允许分支；deny 分支正向文案由 L4 account.sce 对老李头断言
- [x] `abandon`：L2 放弃一个已学技能，断言技能消失（skill_admin.sce 三分支：未 validate 拒绝 / 没学过 / 成功放弃读书写字）
- [x] `enforce`：L2 激发/强化技能分支（enforce_disabled.sce 未 enable 拒绝 + skill_admin.sce 格式/超限/成功三态）
- [x] `nick_name`：L4 设置绰号，断言本人回执（account.sce）
- [x] `who`：任一道 `/who`，断言在线列表含自己（多账号并行下天然有多人）
      —— 实现修正：who 回执是协议级 USERLIST（CLI 只填 online_users_、无文本行），
      断言取「不出现未知命令」证明命令已被服务端注册识别；「在线列表含自己」无文本断言点

### CharState（死亡/复活链，与 P4 互动 PK 合并考虑）
- [x] `l_die`/`l_die_base`/`l_revive`/`l_unconcious`：**按本节标题约定并入 P4**（§4 已有
      「死亡→复活链（P1 遗留项在此收口）」），P1 不做——单账号战斗中被打死的触发成本高

### 待确认（不阻塞 P1）
- [x] `CCmdUser::info`/`CCmdUser::item` **已确认为死代码**：grep `CmdUserInit.cpp` 注册表
      仅有 skill/downloadskill/who 等，**无 info/item 条目**（实现与 flush_my_info/
      flush_my_equip 仍在且被其他路径调用，仅这两个命令入口从未注册）。
      落地二选一（补注册+单测 / 删除并剔出未命中清单）留 P5（§5 已有对应项）

### 稳定性修复：DB 密集写撞 busy 边界（run2 红根因）
- [x] 根因：服务端单线程心跳（`ServerSockWorker.cpp` `GetTickCount-last_tick>=1000`），
      任何把工作线程停顿 >1.2s 的 DB 写（`/quit` 的 `save_to_server` 全量落库、
      改密链的 verify 查询）都可能让前一条 heal 的 `start_busy(2)` 撑过 `sleep 3200`，
      使紧随其后的 busy 校验命令（`/learn`、`/sleep`）撞「你现在正忙着呢」。
      run2 红点 = L4 道尾 quit（约 150-160s）撞 L3 `levelup_heal`→`force_for_exercise`
      首次 `/learn`（约 161s）；run1 的 L4 提前失败未跑到 quit，故 L3 未复现。
- [x] 修复（三处，均不把重写挪进任何道的 busy 窗口）：
      ① `/quit` 从 L4 尾部移到 L1（lane_foot）——L1 约 25~30s 收尾，
        L2 首个 heal 边界约 48s、L3 约 150s 都未到，全道无 busy 边界；
      ② `social/account.sce` 的 DB 密集段（save/改密链）前移到 provision 之后
        （约 20~35s，全道唯一无 busy 边界的窗口），mudage 及以后均为无 DB 步；
      ③ `levelup_heal.sce`/`levelup_rest.sce` 的 heal 后 `sleep 3200→5500`
        （busy 余量 1.2s→3.2s；L3 关键路径 +2.3s×2，在 10s 回归预算内）。
      不采用「learn 容忍 busy 文案 + repeat 余量」方案：force/throw 链恰好
      10 次 learn 对 10 级门槛零余量，吞一次即掉级，且加 repeat 会抬高关键路径耗时。

### P1 验收
- [x] `.\run-integration-tests.ps1` 全绿（run3：L1 275 / L2 496 / L3 519 / L4 300 步，四道 PASS）
- [x] `coverage_history/summary.csv` delta ≥ 0，未命中数较 458 有下降（记录数字）
      —— 实测（E2E 口径，`coverage_history/summary-e2e-sample.txt`）：
      未命中函数 458 → **359（-99）**；行覆盖率 50.1% → **57.5%（8766/15252）**；
      总耗时 387s vs 基线 382s（Δ+5s ≤ 10s 阈值，无耗时回归告警）

---

## 2. 阶段 P2：多账号互动基础设施

多进程（`-Workers` 多 CLI 并行）已就位，但各道**刻意互不依赖**（见 AGENTS.md 隔离原则）。
互动测试需要两道会合，先补基础设施。

- [x] **peer 变量注入**：`run-integration-tests.ps1` 注册阶段已生成全部角色名，
      把 `peer=对方角色名` 写进配对两道的 vars 文件（复用现有 vars 机制，无需改 CLI；
      `$Lanes` 新增 `PeerId` 互指字段，只启用一半配对道时启动即报错）
- [x] **会合同步验证**：会合房间定「山脚茶棚」（暴露面最小：仅 L1 装水片段短暂经过、
      断言全为本人版）；`interaction/rendezvous.sce`：双方 `wait room` +
      `wait object $peer`（object 条件覆盖玩家，服务端 on_enter 双向推 ADDOBJECT）+
      宽 timeout（room 30s / object 60s）——6 道实测两轮无抖动，后到者进门瞬间双方同时满足
- [x] **备选（仅在松同步抖动时做）**：CLI 新增 `sync <barrier>` 文件屏障动词
      + `cli/tests` 单测 —— **不做**：松同步 6 道实测无抖动，无触发条件
- [x] **道模型扩展**：`$Lanes` 增加两条配对互动道（`lane_interaction_a.sce` /
      `lane_interaction_b.sce`），`Workers` 默认 4→6（`ip_limit=30` 够用）
- [x] **互动断言纪律**写进 `.claude/rules/integration-test.instructions.md`（新增 §8.6 五条
      + §5 配对 peer 机制 + §9 对端未到位排查行）：
      ① 私发通道（tell/菜单/notify_fail）优先；② 房间版断言必须含对方角色名（runhex 天然唯一）；
      ③ 会合房间避开其他道动线；④ 一方超时失败信息要写明"疑似对端未到位"；⑤ 稀缺资源归属
- [x] 互动道稀缺资源归属声明（写进 `lane_interaction_a/b.sce` 头注释与 §8.6 ⑤：
      互动道不得触碰木箱/药箱/树箱/山鸡/排行榜等钉给唯一道的资源）

### P2 验收
- [x] 6 道并行全绿（互动道先跑一个空会合片段验证同步）
      —— 实测两轮：首轮 L1~L4 PASS（剧本路径笔误 `interaction_a.sce` →
      `lane_interaction_a.sce` 修复后）复跑 **6 道全 PASS**：L1 275 / L2 496 /
      L3 519 / L4 300 / L5 8 / L6 8 步；L5/L6 vars 确认 `peer=` 互注正确，
      `wait object $peer` 双向通过（会合真实发生，非空跑）
- [x] 纪律文档已更新（§5 道模型六条道 + §8.6 互动断言纪律 + §9 排查行 + §10 清单条目）

---

## 3. 阶段 P3：互动道 P1/P2（社交互动）

**决策（默认）**：方案 A——新增 L5/L6 配对互动道与四道并行。
若 P2 验证发现并行耦合抖动不可接受，退方案 B：四道通过后串行跑互动阶段。

### 组队全链（覆盖 `CmdUserParty.cpp` 177 LOC，当前 0 命中）
- [ ] `party`：A 邀请 B，B `wait menu` + `menu 接受…`，断言双方私发回执
- [ ] `partylist`：队长查在线队员
- [ ] `partyfollow`：B 开启/取消组队跟随
- [ ] `fight` 战斗内跟随行为（`auto_follow_fight` 配置已开）
- [ ] `partyleave`：B 主动离队，断言双方收到取消播报（本人版）
- [ ] `partykick`：A 踢 B，再重新组队恢复
- [ ] **错误分支**：对未邀请者 `/accept_party`、已在队中再邀请、邀请自己

### 私聊与屏蔽
- [ ] `tell`：A 私聊 B，双方断言【私聊】私发回执
- [ ] `refuse_tell` + `tell` 拒绝分支：B 屏蔽 A → A 收到「不接受你的悄悄话」
- [ ] `listen` / `refuse`：屏蔽开关的恢复与再验证

### 定向说话/表情（玩家对玩家）
- [ ] `say` 定向版：A `/t B` + `/say …`，B 断言「对你说」版渲染
- [ ] `relay_say` 玩家版：B 对 A 的定向说话产生反应（现只覆盖 NPC 老李头）
- [ ] `do_emote` 玩家版：A 向 B 发表情，B 断言送达

### 玩家间赠送
- [ ] `give`/`do_give` 用户分支：A `/t 物品` + `/give B`，B 断言 `item` 条件出现

### 做客邀请
- [ ] `invite`：互动道**自行置业**（复用 `house/apply.sce`，不依赖 L1 状态）→ A 邀请 B
- [ ] `accept`：B 接受邀请，断言双方回执

### 结婚/离婚（住宅区月老，`sample_marriage.lua`）
- [ ] `marry`：A 给月老钱领「婚姻登记表」→ A `/use` 签名 → 交给同房间的 B `/use` 签名 → 断言双方 `couple` 映射建立 + `message_vision` 播报
- [ ] `marry` 错误分支：已婚拒绝领表/签名 → 自己重复签名 → 签名人不在场 → 对方已婚 → 金额不足；签名逾时淡去（`fade_sign`）
- [ ] `divorce`：A 离婚咨询 → 确认菜单 → 断言双方 `couple` 删除 + `couple_betray+1` + 房间播报
- [ ] `divorce` 错误分支：未婚拒绝 / 伴侣离线 / 取消确认

### 摆摊交易（`list_goods`/`buy`）——trade.lua 已实现 `trade/value` 入口
- [ ] **摊主定价**：互动道 A 使用贸易文书 `/use trade` → `value <obj_id> <price>` 给多件物品定价，断言 `tell_object`「完成定价！」+ 再次 `list_trade` 看到价格
- [ ] `list_goods`：互动道 B `/t A` + `/list_goods`，断言菜单出现 A 的定价商品列表
- [ ] `buy`：B `menu 购买 <商品>`，断言双方私发回执 + B `item` 条件获得物品 + A 金币增加
- [ ] **错误分支**：买不存在商品 / 买已售罄 / 金币不足 / 对非摊主 `list_goods` / 非玩家摊主
- [ ] **摊主下架**：A `value <obj_id> 0` 取消出售，B 再次 `list_goods` 不再出现

### P3 验收
- [ ] 6 道并行全绿，连续跑 ≥3 次无抖动
- [ ] `CmdUserParty.cpp` 全部函数命中（未命中清单中该文件清零）
- [ ] `pair`（夫妻）：依赖 `couple/id`
- [ ] 摆摊相关 C++ 函数（`CmdStandardItem::list_goods`/`buy` 等）命中
- [ ] delta ≥ 0

---

## 4. 阶段 P4：互动道 P3（玩家 PK 与死亡链）

前置：P3 稳定。PK 段风险高（死亡状态、战利品、惩罚），放最后。

- [ ] `fight` 玩家切磋：A `/t B` + `/fight`，覆盖 `pending/fight` 与 `do_bishi`→CFightRoom
      比试房间链（FightRoom.cpp 多函数 0 命中）
- [ ] `join_battle` / `guard`：第三方观战与加入（可借第 3 个互动账号或复用道）
- [ ] `follow` / `halt`：跟随取消与战斗中断
- [ ] `kill` 玩家 PK：覆盖 `CReward::pk_reward/victim_lose/victim_penalty`、
      `AutoFight`（hatred/vendetta 触发条件）、`ready_pk`
- [ ] 死亡→复活链：`l_die`/`l_revive`/`l_unconcious`（P1 遗留项在此收口）
- [ ] PK 后状态清理：确认惩罚/战利品不影响后续道（互动道独占，风险可控）

### P4 验收
- [ ] 全绿 ×3；死亡账号可正常复活继续或安全下线
- [ ] `CReward`/`AutoFight`/`FightRoom` 未命中函数显著下降
- [ ] delta ≥ 0

---

## 5. 阶段 P5：收尾与清理

- [ ] `vote` 前置验证：VOTE_AGE_LIMIT=15 且 VOTE_LEVEL_LIMIT=5，新号 age=16 已过年龄
      门槛——只剩升级到 5 级一道坎；可测则补 `vote/determine_total/counting/nullify_vote` 片段，
      否则记录为"需长期运营账号，E2E 不测"
- [ ] `mv`/`loc`：评估客户端协议类覆盖价值，低则记录不测
- [ ] `info`/`item` 死代码去留落地（P1 遗留）
- [ ] 补写 `docs/e2e_sample_test.md`（未命中清单头部引用了该文件但文件不存在），
      或改 `run-integration-tests.ps1` 中的引用
- [ ] 覆盖率回归：对比基线，未命中函数总数与 cmd 目录覆盖率记录在案

---

## 6. 明确不测清单（决策记录）

| 项 | 理由 |
|---|---|
| **巫师指令全部**（CmdWizardSystem 307 / CmdWizardAdmin 171 / CmdWizardWorld 92 / `snoop` / SnoopSystem） | 决策：不测巫师指令。这些文件保持 0 命中，属可接受缺口 |
| LuaSyntaxChecker 659 LOC | 只在 `--check-lua` 命令行模式运行，单测已覆盖（`test_lua_checker.cpp`） |
| SqliteDb | E2E 用 MySQL，sqlite 路径单测覆盖 |
| CreateDump / Logger / HeartBeat alarm / db 断连 | 基础设施生命周期，单测兜底 |
| `l_*` Lua 绑定中教材未调用的部分（158 个的大头） | 教材是教材不是测试夹具，不为测试加调用；教材该有而没有的功能另行提内容需求 |

---

## 7. 总验收（全部阶段完成后）

- [ ] `.\run-integration-tests.ps1` 全绿（6 道并行）
- [ ] `summary.csv` 每阶段 delta ≥ 0
- [ ] 未命中函数：458 → 目标 ≤ 330（达成后在此记录实际值：____）
- [ ] cmd 目录行覆盖率：25.5% → 目标 ≥ 35%（记录实际值：____）
- [ ] 每次提交附覆盖率数据（AGENTS/单测规则要求）

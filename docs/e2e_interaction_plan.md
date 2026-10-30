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
      `$Lanes` 新增 `PeerId` 互指字段，只启用一半配对道时启动即报错）。
      **后续演进（/t 契约修正时）**：注册后再回读 acct 互注 `peer_acct=对方数字
      账号 ID`——`/t` 只收 ID（玩家=数字账号 ID 全服跨房间；NPC/物品=英文 id
      当前环境），`$peer` 仅保留给文本断言
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
      ——**后续演进（P3 末 /t 契约修正）**：§8.6 现为六条，新增 ⑥「`/t` 只收 ID」；
      §5 peer 机制补 `peer_acct`（数字账号 ID，供 /t 定向）
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

> **状态（2026-10-29 核实并更新）**：组队全链 / 私聊屏蔽 / 定向说话表情 /
> 玩家间赠送 / 做客邀请 **五段已实现**——片段在 `interaction/{party,tell,
> say_emote,give,invite}_{a,b}.sce`，已挂入 `lane_interaction_a/b.sce`；
> 最近三轮 6 道运行全 PASS（2026-10-29 01:14 / 01:26 含覆盖率轮 / 10:46，
> L5 113 步、L6 59 步）。**剩余**：结婚/离婚 + `pair` 夫妻组队、摆摊交易、
> 个别错误分支（邀请自己）。原勾选状态全部为 `[ ]` 系前任漏更，本节已按
> 实际覆盖逐条改为 `[x]` 并注明片段与断言方式。

### 组队全链（覆盖 `CmdUserParty.cpp` 177 LOC，当前 0 命中）
- [x] `party`：A 邀请 B，B `wait menu` + `menu 接受…`，断言双方私发回执
      （`party_a.sce` step1「接受了你的组队邀请」/ `party_b.sce`「你接受了组队邀请」）
- [x] `partylist`：队长查在线队员（`party_a.sce`「你所处队伍中有以下成员」+「成员」；
      松同步：B 接受后保持在队中，等 A 私发信号「可以离队了」才开始跟随+离队，
      避免 partylist 与 partyleave 竞态）
- [x] `partyfollow`：B 开启/取消组队跟随（`party_b.sce`「决定跟随」/「组队跟随」）
- [x] `fight` 战斗内跟随行为（`auto_follow_fight` 配对跟随进战斗）——**决策收口**：
      两条拖入分支（好友随战入队 / 师徒链邀请）已由单测钉住
      （`test_fightroom_team` 的 join_team 友随战与 master chain 用例）；
      E2E 不引入：把组队/夫妻状态拖进 PK 战斗房会让入队方落入两队籍
      （`join_team` 无跨队去重），破坏赢家账号的确定性——P4 的 PK 段在
      组队解散后的干净状态下进行（见 §4）
- [x] `partyleave`：B 主动离队，断言双方收到取消播报（本人版「你取消了与」/
      私发「<名>取消了与你的组队」，`party_a/b.sce`）
- [x] `partykick`：A 踢 B，再重新组队恢复（`party_a.sce` 二段组队 + 「你取消与」；
      两处 `clear` 清旧回执防假匹配）
- [x] **错误分支**（部分）：已在队中再邀请（「对方已经接受了你的邀请」，A）+
      对未邀请者 `/accept_party`（「对方并没有邀请你」，B，放在队伍解散后避免竞态）
- [x] **错误分支**：邀请自己（`party_a.sce` 末尾 `/t $acct`（CLI 解析为自身
      player_id）+ `/party` →「你对自己说：算了还是自己邀请自己吧。」）

### 私聊与屏蔽
- [x] `tell`：A 私聊 B，双方断言【私聊】私发回执（`tell_a/b.sce` 六步握手；
      信号用反方向 tell 与房间版 say，`refusetell` 只拦 A→B 方向、互不干扰）
- [x] `refusetell` + `tell` 拒绝分支：B 屏蔽 A → A 收到「不接受你的悄悄话」
      （命令实名为 `refusetell`/`wantlisten`，注册表 `CmdUserInit.cpp`，
      原计划写的 `refuse`/`listen` 是函数名不是命令名）
- [x] `wantlisten`：屏蔽恢复与再验证（B「你开始收听」→ A 再发「悄悄话丙」成功；
      第三次 `clear` 清旧「你告诉」确保断的是真成功）

### 定向说话/表情（玩家对玩家）
- [x] `say` 定向版：A `/t B` + `/say …`，B 断言「对你说道」版渲染（`say_emote_a/b.sce`）
- [x] `relay_say` 玩家版：随定向 say 顺带触发（玩家版为空函数、无可断言文案，
      仅计覆盖率；老李头 NPC 版已由 L2 覆盖）
- [x] `do_emote` 玩家版：A 向 B 发 `face` 鬼脸，B 断言目标渲染「对着你顽皮地做了个鬼脸」

### 玩家间赠送
- [x] `give`/`do_give` 用户分支：A `/t B` + `/give 金创药`（CLI give 助手），
      A 断言本人版「给$peer数量」，B 以背包 `item` 条件断言真实到账（§8.6①，
      `give_a/b.sce`；金创药由 L5 自己 provision 购买，见 `interaction_provision_a.sce`）

### 做客邀请
- [x] `invite`：互动道**自行置业**（`invite_a.sce` include `house/apply.sce`，
      不依赖 L1 状态；房名「雅室」为 per-owner 实例不冲突），置业后步行回茶棚邀请
- [x] `accept`：B 接受邀请，断言双方回执（`invite_b.sce` 宽 timeout 等置业+步行）

### 结婚/离婚（住宅区月老，`sample_marriage.lua`）
> **实现方式已变更**：引擎没有 `/marry` 玩家命令；结婚走「付款领**婚姻登记表**
> → 双方两段签名」（`lua-sample/item/sample_marriage_form.lua`，签名记在物品
> 自身上、`fade_sign` 120s 逾时淡去；「和谁结婚」由物品字段+房间在场承载，
> 不要发明 `marry_target` 玩家状态）。月老 `sample_marriage.lua` 挂在
> `sample_resident.lua`（青云城住宅区）。

- [x] `marry`：`interaction/marriage_a/b.sce`——A `/give 金币 100` 月老领表 →
      A `/use` 签名 → `/give` 交 B → **B 持表走到山脚触发「签名人不在这儿」驳回**
      → 回房再 `/use` 完成第二签（B 的不在场回呼是**跨房间 tell 实证**：
      `/t $peer_acct` 按数字账号 ID 全服定向——花名册交换失效时该步即红；
      同房间定向一律 `/t <ID>`，见 §8.6⑥；收敛点是 couple 建立的房间播报，
      第二签者视角「你与<对方>…」、第一签者视角「<对方>与你…」）
- [x] `marry` 错误分支：已婚拒绝领表 / 自己重复签名 / 签名人不在场 / 金额不足
      （1 金币交付 → accept_object 落「非百金」return 0 → do_give 拒收）；
      `fade_sign` 120s 超出 E2E 节奏不测（教材语义自明）
- [x] `divorce`：`interaction/divorce_a/b.sce`——月老菜单「离婚登记」→ 确认菜单
      →「我决心已下」→ 双方断言 `message_vision` 播报 + A `/pair` 落「这不太合适吧？」
      （间接断言 couple 已删）
- [x] `divorce` 错误分支：未婚拒绝（结婚前先做）/ 取消确认（「再考虑考虑」，
      无副作用由后续 pair 仍失败间接复核）；伴侣离线分支需一方 `/quit`、
      破坏配对道结构，不测（决策记录）

### 摆摊交易（`list_goods`/`buy`）——trade.lua 已实现 `trade/value` 入口
> **实现中修复教材 bug**：`lua-default/item/trade.lua` 的 `string.format` 用了
> C 风格 `%ld`——Lua 不支持，`list_trade` 一进就抛「invalid conversion '%l'」、
> `/use` 全线失败（该脚本此前从未被任何测试执行过）。已改 `%d`（4 处）。
> 这是教材正确性 bug，非测试特判。

- [x] **摊主定价**：`interaction/trade_a.sce`——A `/ask 交易` 山脚商人领文书 →
      `/use 贸易文书`（自用分支）→ `menu <商品>` + `menuinput <价>`（菜单路径
      `$2value $1 <obj> $2`，全程无需硬编码 obj_id）→ 断言「完成定价！」+
      重开菜单行「金创药 ：100」
- [x] `listgoods`：B `/t A` + `/listgoods` → 菜单选购（`$0buy $1 <obj>`）
- [x] `buy`：双方 `message_vision` 本人版（买方「你从<peer>那里买下了…」、
      卖方「从你那里买下了…付给你数量100」含定价金额）
- [x] **错误分支**：买不存在商品（`/buy 999999`→「对方不打算卖给你」）/
      金币不足（茶壶定价 2 亿 →「摸摸口袋」）/ 对无定价摊主 `listgoods`
      （下架后空商品分支「商品似乎不感兴趣」）
- [x] **摊主下架**：文书菜单对该商品 `menuinput 0`（「取消出售。」）→
      B 复核 `listgoods` 落空分支

### P3 验收
- [x] 6 道并行全绿，连续 ≥3 次——**2026-10-30 达成**：run6（NoCoverage）、
      run7（带覆盖率，**整轮状态 PASS、无关服崩溃**——插桩改变时序，竞态未触发）、
      run8（NoCoverage）三轮 L1 288 / L2 496 / L3 519 / L4 331 / **L5 262 /
      L6 135 步**全 PASS 且步数完全一致（零抖动），总耗时 391s（Δ-2s 无回归）；
      run6/run8 的整轮 FAIL 仅由下方「关服路径崩溃」独有因素导致（道判定全绿）
- [x] `CmdUserParty.cpp` 全部函数命中——**E2E 未命中清单已清零**（run7 核对
      `e2e-sample-unhit-functions.txt`：CmdUserParty / CmdUserVendor 均 0 条）；
      单测合并口径 177/177 = 100%
- [x] `pair`（夫妻）：`interaction/pair_a/b.sce`——⚠ pair 邀请菜单里是硬编码
      `accept_party`（不带夫妻标记），B 须关菜单后直发 `/accept_pair` 才能覆盖
      夫妻版接受函数；`pairlist`/`pairfollow`/`pairkick`/`pairleave` 全覆盖
      （pairleave 走被踢后的「你并没有加入别人的队伍」错误分支）
- [x] 摆摊相关 C++ 函数（`list_goods`/`buy`）——E2E 未命中清单清零 +
      单测口径 34/34 = 100%
- [x] delta ≥ 0——单测口径 +0.0%（2026-10-30 run-coverage，252 用例全过）；
      E2E 口径（run7）：行覆盖率 **59.4%（9108/15335）**，较 P1 末 57.5% 再 +1.9pt，
      未命中函数 **359 → 338（-21）**，cmd 目录 56.3%

### /t 契约修正（P3 收尾追加，2026-10-30）
> 契约与服务端/CLI 配套已定稿并写入 `docs/design/integration-test.md` §4，
> 此处只留落地记录。
- **剧本**：108 处 `/t <名>` 全量迁移为 ID（NPC/物品英文 id 字典 + `$peer_acct`）；
  `run-integration-tests.ps1` 注册后互注 `peer_acct`（`$peer` 仅留文本断言）；
  教材 `sample_mountain_foot.lua` 箱子补独立 id（`mu xiang`/`yao xiang`——单参
  `set_name` 会把 id 固化成空串）。
- **实证**：`marriage_b.sce` 在山脚（跨房间）`/t $peer_acct` + `/tell` 回呼、
  A 侧断言到达；`run-coverage.ps1` 257 用例全过、delta +0.1%；
  E2E 六道全 PASS（L5 264 / L6 141 步）。纪律与机制写入指令文件 §8.6⑥ / §5 peer 机制。

### ⚠ 遗留阻塞（非 P3 引入，独立生产 bug）：关服路径崩溃
- 现象：**所有道 PASS 后**，服务器在关服收尾阶段抛未处理异常（0xC0000005
  **取指型故障**，故障地址每次不同——野函数指针/悬空虚调用特征），脚本判 FAIL。
- 定位（2026-10-30）：
  - **30 秒确定性复现**：全新账号登录后静置 → WM_CLOSE 优雅关服 → 必崩；
    仅游客在线则不崩——触发条件是「有真实用户在 `g_user_list` 中走
    `user_release`」。
  - 崩溃线程 = `main_worker_thread`（`g_terminated` 置位后在工作线程执行
    `exit_game()`：user_release→npc_release→item_release→room_release）；
    栈上有 `CMapping::to_json`（dbase 序列化）等帧（/OPT:ICF 下符号有折叠，
    仅供参考）。
  - 引入窗口：最后一次绿跑（10-29 10:46，源码 48ebbb2）之后、提交 3dc928d
    （10-29 23:40）的树之间——注意 3dc928d 同时修了 vcxproj 库搜索顺序
    （`$(OutDir)` 优先），此前 E2E 可能链到 `bin\` 旧 `jianghu_core.lib，
    故更早的「绿」未必反映当时源码。
  - **时序依赖**：同树的带覆盖率轮（OpenCppCoverage 插桩、进程更慢）
    **不崩**——竞态型 UAF 的又一佐证；NoCoverage 轮 6/6 必崩。
  - dump 在仓库根/`crash_dumps\`；分析工具（读 dump + PDB 符号化）：
    `%TEMP%\opencode\dumpwalk\dumpwalk.exe <dump>`（须用系统 dbghelp）。
- 处置：**独立生产 bug 修复**（需设计级修复 + 单测），不并入 P3；
  在修复落地前，E2E 全绿判定需人工区分「道全 PASS + 仅关服崩溃」。

---

## 4. 阶段 P4：互动道 P3（玩家 PK 与死亡链）

前置：P3 稳定。PK 段风险高（死亡状态、战利品、惩罚），放最后。

> **状态（2026-10-30 实施）**：本阶段先解决了一处**出手准入的架构双轨问题**
> （`CCmdStandard::kill` 与 `CChar::cmd_hit_ob` 两处准入并存），按 CombatRules
> 既有契约统一为 `cmd_hit_ob` 单一询问 `attack_gate`——设计裁定已写入
> `docs/design/integration-test.md` §3。由此 `kill` 玩家 PK、死亡链、
> `join_battle` 成功分支在 E2E 变为可达。道编排新增 **L7 观战道**（`lane_spectate.sce`，
> L5 `WitnessId=7` / L7 `PeerId=5` 双向注入 `spectator*`/`peer*` 四键），
> `-Workers` 默认 6→7（Workers 变更 → 耗时历史记新基线）。

### fight 玩家切磋（A↔B，`interaction/fight_a.sce` / `fight_b.sce`）
- [x] `fight`：A `/t $peer_acct` + `/fight` 落 `pending/fight`（双端私发挑战语）→
      B 回 `/fight` → `do_bishi` → threshold 比试房（回合推进 / AP 累积 /
      双端进战）→ A `/halt`「跳出战圈」→ `end_fight` 双方回茶棚
- [x] kill 拒绝分支：比试房内 A `/kill` B →「不能在观战中暗中偷袭」
      （B 已挂 `fight/is_fighting`）
- 决策：比试打到 50% 血触发 `is_defeated` 的耗时取决于无技能对殴的伤害节奏，
  不作 E2E 断言（`test_bind_fightroom` HeartBeatBishiYield 等单测已钉该分支），
  收场走可确定的「跳出战圈」

### join_battle / guard（第三方 = 新增 L7 观战道，`interaction/spectate.sce`）
- [x] `guard`：L7 收 A 跨房 tell 信号 → `/t fight_room` + `/guard` 真进比试房
      （本人版「你加入观战」）→ `/halt` 观战退出（「你离开了…」）
- [x] `join_battle`：PK 战斗房（非 threshold）`can_join` 通过 →「<名>加入战队0」
      + `CUser::ready_pk`（L7 入队）

### follow / halt（`interaction/fight_a.sce`）
- [x] `follow` 开（双端房间版渲染）与 `/t $acct` 取消（私发「取消跟随！」）
- [x] `halt` 三分支：不忙拒绝（notify_fail）/ 比试 threshold「跳出战圈」/
      L7 观战退出；忙碌中断由 L3 打坐段覆盖（P1 既有）

### kill 玩家 PK（`interaction/pk_a.sce` / `pk_b.sce`）
- [x] **PK 前置（P4 迭代定稿：「赢家 10 级、输家 1 级，只打在赢家账号上」）**：
      `interaction/pk_prep.sce`（仅 L5）——起步历练 5 万
      （`common/room_look_inquiry`，按 reward/exp 每号一次）→ 自购青锋剑
      （商品 2，`apply/damage=30`）并装备 → `/upgrade`×9 冲到 **10 级**
      （`level2_exp(10)=7500`，5 万绰绰有余；对 1 级输家差 9，压在
      `couple_level_diff=10` 之内不伤婚姻段）。**刻意不学技能**：sword 每级
      只 +skill/4 伤害却要师父/回春丹/气血三重耦合——收益最小、面最大，弃用；
      L7 观战道完全退出准备（零采购零学艺），准备对其余道的耦合只剩
      「商人多卖 1 柄剑」。命中杠杆：ap=combat_exp/2≈5万 vs 徒手 dp=0 必中，
      单拍 ~48 → 1 级 342 气血的 B ≈ 7 拍（A 单挑 30s 内）。
      迭代中另修 CLI 真 bug：RTNITEM 分批下发（每 10 件、批头 `B=` 为全局
      偏移），客户端原从 0 解析导致**第二批整体丢弃**——第 10 件起的物品
      装备状态永远收不到（`wait wield` 连败两轮的根因，`cli/tests`
      `ReceiveItemWieldPositionsHonorBatchOffset` 钉住）。
- [x] 成功路径：A `/kill` B → 非 threshold 战斗房（`begin_fight(0)`）→
      `ready_pk` → 结算 `victim_lose` → `handle_pk_reward` → `pk_reward`。
      **收敛点（实测后修正）**：pk 私发「你PK玩家…」只到**致命一击者**，
      而最后一击落在 A 还是观战道是随机的——不能押；改为双侧确定断言：
      A 断房版死亡播报（`<B>死了。`，die() 首行、战斗房内三方必达），
      B 断 `handle_pk_reward` 的 rumor 尾（`杀了，损失`）——**执行序上
      rumor 在 `pk_reward` 之后**，断到即证整条 PK 结算链跑完。
      实测致死 251s（闪避/招架损耗 + 昏迷后 eff_hp 慢穿底），各 wait 窗口
      按 360s/60s 放宽——首版 240s 曾以 9~11s 之差让 A/C 超时先退、
      死讯发给已离线者翻车（服务器日志 `handle_pk_reward 13:03:29`
      晚于 A 断线 `13:03:20`）
- [x] `CFightRoom::do_flee`：B 死亡结算后 A `/halt` 逃离（非 threshold、AP 已
      500 封顶）；L7 入队后**不撤场**——全程留队双打，把致死从单挑 ~160s 压到
      ~100s，散场由 `end_fight` 请出（收尾断言走 `do_leave_team` 私发
      「此次战斗耗时」；单人 405 气血对殴推算见 pk_b 头注）
- [x] `CReward::victim_penalty`：**决策已定（不测，单测兜底）**——需要「NPC 击杀玩家」
      或无凶手死亡。样本世界可
      战斗房间的 NPC 全部钉给唯一道（半山腰木人/师父=L2、银行=L4、山顶山鸡与
      副本山贼=L3），互动道没有可输的对手。决策：**不为测试挪动资源归属**
      （§8.6⑤），由单测兜底（`test_reward` KillerReward_NpcSlaysUser_PenaltyApplied）
- [x] `AutoFight` hatred/vendetta/aggressive/berserk：**决策已定（不测，单测 18 例兜底）**
      ——hatred 需「打过 NPC 后
      再次撞面」、aggressive 的进取型 NPC 只在副本（L3 独占进出）、vendetta
      教材无人设置 `vendetta_mark`、berserk 无触发源——E2E 没有合法触发面。
      决策：**不测**（单测 18 例已覆盖分支，`test_auto_fight`）

### 死亡→复活链（P1 遗留项在此收口）
- [x] B 侧自然链：战斗中 `hp<0` 而 `eff_hp` 未穿底（伤口按 ~25% 拍率削 eff_hp、
      hp 每拍都掉，先昏迷）→ `CChar::unconcious`（置 `block_msg/all` **之前**
      私发「你的眼前一黑…」）→ 下一拍 `hp<0` 且非存活 → `CChar::die`（user 分支
      先 `revive(quiet=1)` 解 block、清队私发「此次战斗耗时」、迁 `start_city`
      = 青云山山脚、`hp=1`/`eff_hp=max_hp/2`）
- [x] 断言纪律：B 全取私发/本人状态（`你的眼前一黑` / `此次战斗耗时` /
      `battle_end` / `attr hp <= 1` / `wait room`）；`<名>死了。` 房间播报在
      block 期间会被拦，双端都不依赖
- [x] `l_die`/`l_revive`/`l_unconcious`（Lua 绑定）：教材无调用方，属 §6
      「教材未调用」类，不为测试加调用；其 C++ 对应物由本链覆盖
      （`CChar::die/unconcious/revive` 此前已被 L3 山鸡链命中，本链补 user 分支）

### PK 后状态清理
- [x] 尸体随 `end_fight` 落茶棚，120 拍腐烂只发本房间（腐烂断言钉在 L3 山顶，
      彼时各道均已收工）；PK 频道文案（行凶杀死 / 被…杀了）已 grep 全部道无
      同子串断言；B 死亡迁 `start_city`、A 撤出即散房，均处互动道链尾，
      不触碰任何钉给唯一道的资源

### 实施期修复的跨道竞态（round9，教训固化为 §8.6⑦）
- A 曾快于 B 完成离婚：播报「与你解除了夫妻关系」先到 B 缓冲，被 B 的 pair 收尾
  `clear` 抹掉 → divorce_b 干等超时 → A（等不到买药）/L7（等不到开战信号）连锁红。
  修复：pair_b 收尾块移除 `clear`（该 wait 的 needle 只可能由本命令产生，无需清旧）；
  规则固化为 §8.6⑦「`clear` 不得抹掉跨道信号」。（CLI RTNITEM 分批 bug 见上方
  PK 前置条目。）

### P4 验收
- [x] 全绿 ×3：2026-10-30 run10/11/12 连续三轮 **PASS（7/7 道）**，步数
      L1 288 / L2 496 / L3 519 / L4 331 / L5 379 / L6 165 / L7 28
      **三轮完全一致（零抖动）**；总耗时 391s / 388s / 391s，Δ≤3s（阈值 10s，
      workers=7 记为耗时新基线）。死亡账号 B 正常复活：`unconcious → die →
      revive(quiet)` 后迁 start_city（青云山山脚），走完全部收尾断言
      （pk_b 165 步含 rumor 收敛与房态迁移）。PK 致死实测 **35 秒**
      （`此次战斗耗时35秒`；徒手时代 251s）。
- [x] `CReward`/`FightRoom` 未命中函数显著下降：CReward 4 → 2（`victim_lose`、
      `pk_reward` 清零，剩 `victim_penalty`、`l_killer_reward`——上方决策类）；
      FightRoom 的 `can_join`、`do_flee` 清零（剩 `l_*` 绑定与 `get_team_count`
      ——教材未调用类）；`fight`/`guard`/`join_battle`/`follow`/`ready_pk`/
      `do_bishi` 全部从未命中清单清零。E2E 未命中 338 → **321（-17）**，
      行覆盖 59.4% → **61.2%**（combat 45%→55.3%、cmd 56.3%→60.6%、
      user 68.1%→71.6%）。AutoFight×4 与 `is_defeated`/`l_*` 残留为已记录的决策项。
- [x] delta ≥ 0：单测口径 `run-coverage` **Delta +0.0%**（引擎 2154 + CLI 258
      用例全过，含 CLI 批次修复新用例）；E2E 口径 59.4% → 61.2%（+1.8pt）。

---

## 5. 阶段 P5：收尾与清理

- [ ] `vote` 前置验证：VOTE_AGE_LIMIT=15 且 VOTE_LEVEL_LIMIT=5，新号 age=16 已过年龄
      门槛——只剩升级到 5 级一道坎；可测则补 `vote/determine_total/counting/nullify_vote` 片段，
      否则记录为"需长期运营账号，E2E 不测"
- [ ] `mv`/`loc`：评估客户端协议类覆盖价值，低则记录不测
- [ ] `info`/`item` 死代码去留落地（P1 遗留）
- [x] ~~补写 `docs/e2e_sample_test.md`~~ **销账**：未命中清单的口径与局限已并入
      `docs/design/integration-test.md` §5.3，`run-integration-tests.ps1` 两处引用已改指该文档，不再新建该文件
- [ ] 覆盖率回归：对比基线，未命中函数总数与 cmd 目录覆盖率记录在案

---

## 6. 明确不测清单（决策记录）

> 完整清单与理由已定稿于 `docs/design/integration-test.md` §6（含 victim_penalty、
> AutoFight、auto_follow_fight 等后续追加的决策项）。此节仅存当时的基线快照：

| 项 | 理由 |
|---|---|
| **巫师指令全部**（CmdWizardSystem 307 / CmdWizardAdmin 171 / CmdWizardWorld 92 / `snoop` / SnoopSystem） | 决策：不测巫师指令。这些文件保持 0 命中，属可接受缺口 |
| LuaSyntaxChecker 659 LOC | 只在 `--check-lua` 命令行模式运行，单测已覆盖（`test_lua_checker.cpp`） |
| SqliteDb | E2E 用 MySQL，sqlite 路径单测覆盖 |
| CreateDump / Logger / HeartBeat alarm / db 断连 | 基础设施生命周期，单测兜底 |
| `l_*` Lua 绑定中教材未调用的部分（158 个的大头） | 教材是教材不是测试夹具，不为测试加调用；教材该有而没有的功能另行提内容需求 |

---

## 7. 总验收（全部阶段完成后）

- [ ] `.\run-integration-tests.ps1` 全绿（7 道并行）
- [ ] `summary.csv` 每阶段 delta ≥ 0
- [ ] 未命中函数：458 → 目标 ≤ 330（达成后在此记录实际值：____）
- [ ] cmd 目录行覆盖率：25.5% → 目标 ≥ 35%（记录实际值：____）
- [ ] 每次提交附覆盖率数据（AGENTS/单测规则要求）

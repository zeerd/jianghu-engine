# PLAN：仆人租借存盘（方案 B）+ 重启集成测试

> **计划书声明**：本文件是任务清单（条目 + 勾选 + 一行运行记录），**批次收口后随条目删除**，
> 不作为长期文档存在；`docs/design/`、`.claude/rules/`、剧本头注、脚本注释一律**不得引用本文件**。
> 需要留存的设计内容在实施时迁入 `docs/design/`（设计粒度，见 E 组）。
>
> **状态：设计已定稿，等命令实施。** 未经用户下令不动代码。

---

## 0. 已定设计裁定（实施时迁入设计文档，此处只留结论）

- [ ] **D1** 货架状态以**房屋清单**为唯一账本（方案 B）：dbase 字段 `goods`，条目
      `{base,index,amount,rent,price[,onloan]}`；仆人自身**不建存档行**（不碰 `t_npc`、
      不加 `can_save`、不走 `save_table/save_key`）——绕开「key=base_name 全服一行」的
      全局唯一假设（`src/object/ContainerMisc.cpp:148-168`）。
- [ ] **D2** **在租条目留在清单里**（含 `onloan=租客id`）→ 重启/补役时回架，房主不因
      租客不还而丢货；售出/主人取回时**删条目**（不删即复制）。
- [ ] **D3** 租约标记写在**玩家 dbase**（`rent/*`：房号+base/index/amount）——物品的
      temp 与 dbase 都不入玩家存档（`auto_load` 只有 base_name/index/amount/unique，
      `src/char/CharSave.cpp:186-194`），玩家存档是唯一能从登录侧反查的载体。
- [ ] **D4** **登录即删副本**：`login.on_login` 在玩家进世界之前（`src/net/Login.cpp:493`，
      钩子后 `:500` 立即 `me->save()`）按标记定位副本 → 无 `yongren/rent` 标（跨重启）→
      `destruct` + 私发通知；有标（同会话重连）→ 不动；找不到（已归还/已消耗）→ 清标记。
      依据：防盗机制是**房间级**（`house_base.lua:226-236` valid_leave、`:443` intercept），
      重启后玩家落回 `start_city`（`Login.cpp:293-298`）没有生效环境，副本存活即白送。
- [ ] **D5** **重建规则**：物化清单条目时，仅当「`onloan` 租客**在线且背包确有匹配副本**」
      才跳过该条目，其余照常恢复——否则运行期补役（`house_base.lua:196-198`）会造出
      与在线租客并存的第二件 → 归还后永久复制。判定用现成 `find_online` + `Get_ItemList`。
- [ ] **D6** **不做租金退款**（服务器异常重启属不可控事件，用户裁定）。
- [ ] **D7** `rent/price` 由 temp 迁 dbase（仅**物品身上**的；玩家身上挂货流程的瞬态
      `me:set_temp` 保持 temp）。

---

## A. 生产侧（纯 Lua，`lua-default/`）

- [ ] **A1** `lua-default/room/house_base.lua`：清单读写与物化
      - `make_yongren` 按 `goods` 物化（`load_item` + `set_amount` + `move` + 重放
        rent/price 到物品 dbase），逐条套 D5 跳过规则；键允许留洞，遍历用 `querymap_keys`
        （`resident_base.lua:35` 同款）。
- [ ] **A2** `lua-default/npc/monster_yongren.lua` 四个变更点（沿用现有
      `LockData(1)/Load/Save/LockData(0)` 房屋落库模式，`:377-384` 等）：
      - `do_price`（`:318-334`）：清单加条目；
      - `do_giveme`（`:199-250`）：主人取回 → 删条目；买（rent=2）→ 删条目；
        租（rent=1）→ 条目置 `onloan` + 玩家 dbase 写标记 + `me:Save()`（菜单命令路径
        无引擎自动存盘）；
      - `accept_object`（`:514-519`）：归还 → 校验标记属本房 → 清玩家标记 + 清 `onloan`
        （随后 `do_give` 的 `me->save()`，`src/cmd/CmdStandardItem.cpp:370` 兜底落库）；
      - `do_getobj/do_giveme` 的 `query_temp`（`:185,187,214,223`）与 `:326-327` 的
        `set_temp` → dbase（D7）。
- [ ] **A3** 租约清理钩子：`login.on_login`（`lua-default/data/login.lua:26`）内加一行调用，
      逻辑放独立模块（如 `lua-default/room/rent_registry.lua`），保持 login.lua 简单。
      玩家 dbase 标记的增删与 A2 同步。
- [ ] **A4** `lua-default/room/house_rent.lua`：`RENT_FLAG`/`is_rented`（`:11,24`）改读
      物品 dbase；拦截逻辑本身不动。
- [ ] **A5** 生命周期核对（记录进设计文档）：房屋注销 `house:remove()` 已删房行
      （`resident_base.lua:192-194`）→ 清单随行消失；热重载 `reload monster_yongren` 的
      `purge()` 只删声明了 `save_table` 的行（`src/lua/LuaNpc.inl:68-70` +
      `src/object/Container.cpp:127-143`），仆人不声明 → 不受影响。
- [ ] **A6** 生产代码注释禁止出现测试动机/计划书引用（AGENTS.md 铁律）。

## B. 单测（`tests/unit/`，每条先过 test-audit 四问：保护什么/什么回归会红/为何现有
## 覆盖抓不住/是否需要测试专用 seam）

- [ ] **B1** 清单契约：挂货入账、买/取回出账、重建物化与 rent/price 重放（owner 边界：
      照 `test_house.cpp` / `test_resident_lua.cpp` 的驱动方式）。
- [ ] **B2** 重建跳过规则两态：在 fixture Lua 环境内**替换全局 `find_online`** 构造
      「在线+有副本→跳过」「离线→物化」「在线无副本→物化」——不为测试加生产 seam。
- [ ] **B3** 登录钩子三态：有标记+无标副本 → destruct+清标；有标副本 → 不动；
      无副本 → 清标。
- [ ] **B4** 边界：归还清标记与 `onloan`；标记指向已注销房屋 → 清标不炸；
      同型同量歧义（等价物品，吃哪件价值相同）。
- [ ] **B5** 运行 E2E 覆盖不到的两条（B2/B4）是本方案的主要单测价值；重复 setup 合并。

## C. CLI 负向背包断言（**待拍板 G1**）

- [ ] **C1**〔甲·推荐〕`cli/src/scenario_steps.cc` 加 `noitem <名>`（镜像 `item`
      `:84-92` 取反，读 `st.inventory()`）+ `cli/tests/scenario_steps_test.cc` 用例
      （照 `notext` 测试写法）+ design §7.1 条件表加行。
- [ ] **C2**〔乙·零代码〕`clear` + `cmd .inventory`（`cli/src/input.cc:150` 本地面板）+
      `wait notext <名> timeout=3000`。

## D. 集成测试：独立服务器「阶段 3」（不影响任何道）

**隔离四件套：独立端口 + 独立 config + 独立 DB + 独立 run-id 日志文件。**
⚠️ **不共用 `e2e.db`**：SQLite 支持跨进程，但冲突在游戏行级读改写——两台各自持有
内存世界，对同一行 `LockData→Load→改→Save`（住宅区登记行 `resident_base.lua:105-119`、
容量判断 `:90-96`）会互相丢更新，`obj_lock` 跨进程也无约定。独立 DB 还顺带解决
跨轮 capacity 占满与历史房串菜单（每轮全新，房名固定即可）。

- [ ] **D1** 新增 `tests/integration/config_restart.ini`：照抄 `config.ini`，改
      `[server] port=6612`、`[database] path=tests/integration/e2e_restart.db`；
      `[log] path` **保持同目录**（配合 `--log-run-id`，见 D5）；头注写明「禁止与主配置
      共用 DB」及行级理由。
- [ ] **D2** 新增 `tests/integration/cli_restart.conf`：`port=6612`（CLI 无 `--port`，
      `cli/src/config.cc:62` 只从 conf 读）。
- [ ] **D3** ⚠️ **必改陷阱**：`Get-TestServerProcess`（`run-integration-tests.ps1:518-529`）
      第一行无条件优先返回 `$serverProc`（**主服**句柄，不看端口）→ 直接调
      `Stop-ServerGraceful -Port 6612` 会**先杀主服**。改为可选句柄参数（第二台由本脚本
      启动、句柄在手身份无疑，符合进程安全规则），句柄失效才按端口认进程。
- [ ] **D4** 阶段 3 主体（插在阶段 2 判定 pass 之后、`finally` 之前；道失败则跳过）：
      1. 删 `e2e_restart.db(+wal/shm)` → 起第二台（`Start-HiddenProcess`，**不包 OCC**，
         `--config config_restart.ini --log-run-id e2e-restart-<ts>`）→ `Wait-Port 6612`
         （残留回收用 `Stop-OrphanTestServer -Port 6612`）；
      2. `cli_restart.conf` 注册 owner/renter（`register.sce` 并行 → `restart_*.vars`；
         **不能混进阶段 1 的 6611 注册**）；
      3. `prepare_owner` → `prepare_renter`（`Invoke-ParallelBatch` 单项串行）；
      4. 停第二台（D3 句柄/端口）→ 再起 → `Wait-Port`；
      5. `verify_owner` → `verify_renter`；
      6. 停第二台；任一步失败 `throw`（与道同级；道结果此时已判定，不会丢）。
- [ ] **D5** 日志：第二台用 `--log-run-id` → 独立文件 `M-D-e2e-restart-<ts>.txt` 落在
      **同一日志目录**（`src/util/Logger.cpp:77` 追加 `-{run_id}`）；现有
      `Get-ServerLogIssues`（`:923` 遍历目录、运行期新建文件 offset 缺省 0）**自动覆盖，
      无需加 `-LogDir` 参数**；报告点名该文件路径。同一份 `log-allowlist.txt`。
- [ ] **D6** 崩溃：第二台 cwd=仓库根 → dump 落仓库根 → 主 diff 天然兜底；阶段 3 再做
      自有快照 diff，把崩溃归属到阶段 3。
- [ ] **D7** 耗时口径：`$tRestartSec` 计入 `exec_sec`；三行硬指标补
      「+ 重启阶段（串行）」；Δ 归因**先看 `restart_sec` 增量**；预算 420 **不动**
      （当前 full 实测 250s/余量 170s，阶段 3 预计 35~45s）。
- [ ] **D8** `-ResetDb` 连带删 `e2e_restart.db`；`-Simple` 跳过阶段 3（冒烟口径不变）。
- [ ] **D9** 剧本 4 段 `tests/integration/scenarios/restart/`：
      `prepare_owner.sce`（复用 `common/login.sce` + `house/apply.sce`，仿
      `mountain_foot/buy_sell.sce` 采购 **4 包**金创药 → 挂货 A=租200 / B=租300 →
      主人取回 C=租500 → **第 4 包存柜** `/drop jin chuang yao into box`（仿
      `house/box_store.sce:22-23`；存入即落库 `Box.cpp:123`，与关服是否保存无关）→
      `common/quit.sce`）、`prepare_renter.sce`（`/go 青云城住宅区` → 菜单
      `进入<房名>`（`resident_base.lua:263-264` 访客入口）→ talk → 租走 B →
      **买 1 把酒壶作正对照**（D10④））、
      `verify_owner.sce`（落点即自家房：`registerhouse` 把 `start_city` 设成房号
      `resident_base.lua:483`）、`verify_renter.sce`。
- [ ] **D10** 断言清单（三件货同为金创药，**靠价格区分**，菜单文本 `名字 : 价格` 见
      `monster_yongren.lua:186-190`）：

      | # | 契约 | 断言 |
      |---|---|---|
      | 1 | 未成交商品回架 | 菜单含 `金创药 : 200` |
      | 2 | 在租条目全量恢复 | 菜单含 `金创药 : 300` |
      | 3 | 取回件不回架（防复制） | 菜单**不含** `金创药 : 500` + `wait item 金创药`（在背包） |
      | 4 | 租客副本删除（**须配正对照**） | 登录后 `noitem` 金创药（C1/C2）**且** `wait item 酒壶`——否则「整包被清」的 bug 会假通过（test-audit 的负控制垃圾模式） |
      | 5 | 钩子真跑了 | 登录收到私发收回通知 |
      | 6 | 服务端无报错/无崩溃 | 日志扫描 + dump diff（D5/D6 自动） |
      | 7 | **柜内物品跨重启读回**（顺带） | `/t box` → `/look` → 菜单含 `金创药`（房内柜子 `saveobjects=1`，`house_base.lua:159`） |

      > **顺带覆盖的两条零覆盖路径**（本阶段白赚，均无其他 E2E owner）：
      > ① `CBox::load_object` 成功分支（`Box.cpp:179-204`）——现有跑法只走「无 t_box 行」
      > 失败分支（`log-allowlist.txt:24-26`），读回仅在跨重启时可达；
      > ② 玩家 `auto_load` 恢复循环（`CharSave.cpp:221-247`）——每个账号只登录一次
      > （`common/login.sce` 单点），存档后重登从未发生。
      > 覆盖率口径不变：第二台不插桩（E1），故这两条只进断言、不进 `delta`。

## E. 文档同步（设计粒度，不写逐函数细节）

- [ ] **E1** `docs/design/integration-test.md` 新增 §1.5：阶段 3 的阶段模型、隔离四件套、
      不共用 DB 的行级理由、第二台不插桩的覆盖率裁定（纯 Lua 功能 + 方案 B 不引入新
      C++ 路径 → `delta ≥ 0` 可保持）。
- [ ] **E2** `.claude/rules/integration-test.instructions.md`：§3.1「脚本会自动完成」加
      阶段 3；§4.1 条件表加 `noitem`（若 C1 落地）；§10 checklist 加「改动了重启阶段
      （config_restart / 阶段 3 / restart 剧本）→ 加跑完整模式全绿」。
- [ ] **E3** 生产侧设计（清单/玩家标记/重建规则/登录删除）迁入 `docs/design/`。
- [ ] **E4** `docs/e2e-unhit-audit.md` 核对（新 checklist 项；本方案预期无新增未命中）。
- [ ] **E5** 批次收口后**删除本计划书**（连同条目）。

## F. 验收（提交前）

- [ ] **F1** `.\run-coverage.ps1` 全绿，`coverage_history/summary.csv` 的 `delta ≥ 0`。
- [ ] **F2** `.\run-integration-tests.ps1` 全绿（含阶段 3、四通道：剧本/崩溃 dump/
      服务端日志/预算）。
- [ ] **F3** Lua 改动跑 stylua、`ccjh.exe --check-lua` 全量（禁抽样）；C++ 改动跑
      `.\style.ps1`。
- [ ] **F4** 自查：无迁移痕迹注释、无测试动机注释进生产代码、无 BOM、仅 Win32。
- [ ] **F5** 提交由**用户**执行（本 agent 只读 git）。

## G. 待拍板（实施前需用户答复）

- [ ] **G1** 负向背包断言：C1 `noitem`（推荐，CLI 约 10 行 + 单测 + 指令表）还是 C2 零代码？
- [ ] **G2** 关服用优雅关服（`Stop-ServerGraceful`，`WM_DESTROY` 不落盘已核实
      `src/main.cpp:343-359`，正好测「改动即存」）；强杀变体是否排进后续？
- [ ] **G3** 阶段 3 不加跳过开关（与道同权必跑，调试走手跑单剧本）？
- [ ] **G4** 阶段 3 **串行放道之后**（建议：不与 10 道 + OCC 抢 CPU、不动关键路径）还是
      并行（省 40s 墙钟，有预算争用风险）？

---

## 运行记录（一行一条，实施时追加）

（空）

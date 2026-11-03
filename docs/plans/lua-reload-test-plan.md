# Lua 热重载 E2E 集成测试计划

> 本文档是“真实服务器 + 无头 CLI + 剧本驱动”的 E2E 计划，不是单元测试夹具计划。
> 目标：验证热重载在真实运行时是否能稳定覆盖 NPC / Item / Room / base 模块的业务路径。

## 1. 目标

本轮 E2E 计划聚焦下面几类真实场景：

- `reload <脚本名>`：单脚本重载后，角色 / NPC / 物品 / 房间行为是否立刻切换到新脚本
- `reload all`：全量脚本重载后，实例是否仍然保持功能正确
- base / shared 修改：修改 `lua-default` 或共享模块后，后续业务是否拿到新版本且不污染旧实例
- 自动热重载：修改脚本文件后，服务端心跳 / 检测逻辑是否自动重载
- 失败兜底：脚本缺失、语法错误、运行错误时，旧版本是否依然存在，且服务端日志正确输出错误

## 2. 测试架构

本计划遵循集成测试的标准模式：

- 真实服务器：`bin\jianghu.exe`
- 无头 CLI：`bin\cli\jianghu_cli.exe`
- 剧本驱动：`tests\integration\scenarios\*.sce`
- 正式入口：`.\run-integration-tests.ps1`
- 配置：`tests\integration\config.ini`

所有场景必须通过真实 `login` / `cmd` / `wait` / `menu` / `assert` 执行，不允许直接调用单测 API。

## 3. 约束与隔离

- 不得修改正式生产脚本：`lua-default/`、`lua-sample/` 上线脚本一律不改
- 所有临时脚本必须放在 `tests/integration/` 下的专用目录，例如：
  - `tests/integration/reload/`
  - `tests/integration/reload_scripts/`
  - `tests/integration/tmp/`
- 测试结束后必须删除临时脚本与备份文件，避免污染后续 E2E
- 若涉及重载共享 base，必须使用一份临时副本，不能直接改正式 `lua-default` 目录

## 4. 场景清单（任务列表）

### P0：必须先做

- [ ] `reload_basic_single_script`：在真实房间中，改动一个 NPC 脚本后执行 `reload <脚本名>`，断言客户端观察到新行为
- [ ] `reload_basic_item_script`：改动 item 脚本并 reload，验证物品使用效果切换
- [ ] `reload_basic_room_script`：改动 room 脚本并 reload，验证房间事件/描述/门状态切换
- [ ] `reload_all_scripts`：执行 `reload all`，验证全部已加载脚本都保持可用
- [ ] `reload_missing_file`：脚本文件删除后执行 reload，验证服务端保留旧版本而不是崩溃
- [ ] `reload_syntax_error`：写入语法错误脚本并 reload，验证旧版本未丢失，日志有报错
- [ ] `reload_runtime_error`：脚本触发 `error()` / 非法调用，验证 runtime error 不破坏服务器继续运行
- [ ] `reload_keeps_old_version_on_fail`：失败分支必须保留旧引用，随后业务仍然可正常访问

### P1：二轮补齐

- [ ] `reload_base_shared_module`：临时修改共享 base 模块，验证后续业务拿到新值
- [ ] `reload_base_then_new_instance`：共享 base 改后，新实例创建时拿新逻辑，旧实例仍保留旧状态
- [ ] `reload_all_after_base_change`：先改 base，再 `reload all`，验证 `package.loaded` 清理后仍正确加载
- [ ] `reload_auto_detect_mtime_hash`：改写脚本文件并等待心跳，验证自动热重载触发
- [ ] `reload_noop_when_file_unchanged`：写回相同内容时不应误触发 reload
- [ ] `reload_parallel_instances`：多个 NPC/Item/Room 实例同时存在时，单脚本 reload 只刷新对应脚本

### P2：长期回归

- [ ] `reload_large_directory`：多脚本目录改动后批量 reload
- [ ] `reload_followup_after_e2e`：重载后继续走 E2E 剧本验证交互连贯性
- [ ] `reload_with_room_index_refresh`：房间索引 / 地图名 / 房间对象载入在重载后继续有效

## 5. 真实脚本 / 剧本设计

建议新增或扩展：

- `tests/integration/scenarios/reload/reload_basic.sce`
- `tests/integration/scenarios/reload/reload_item.sce`
- `tests/integration/scenarios/reload/reload_room.sce`
- `tests/integration/scenarios/reload/reload_all.sce`
- `tests/integration/scenarios/reload/reload_error.sce`
- `tests/integration/scenarios/reload/reload_base.sce`

每个脚本都遵循 E2E 断言纪律：

- 先 `login`
- 再 `cmd` / `wait` / `menu` / `assert`
- 用 `wait text`/`wait object` 等真实客户端状态断言，不用只断“命令发出去”
- 失败时记录服务端日志路径，查看 `tests/integration/log/` 中本轮 `M-D-<id>.txt`

## 6. 关键断言

### 6.1 正常 reload

- 角色/房间/NPC/物品的 UI、描述和行为发生变化
- 发出 reload 后，下一条业务命令读到新逻辑
- 触发 reload 之后，状态变化可见于实际输出，不止日志中出现“已重载”

### 6.2 失败兜底

- 脚本文件缺失时，服务器不崩溃，旧版本继续工作
- 语法错误时，旧实例继续存在，新脚本不覆盖旧状态
- `reload all` 中一个脚本出错，不应导致全局失效
- 服务端日志中记录脚本名与错误消息，且必须能定位到具体文件

### 6.3 base / shared

- 修改 base 后，`require` 取到新版本
- 新生成的实例走新逻辑
- 旧已有实例不被错误地“全量替换掉”
- 全量 reload 后，不出现业务对象失效或状态串扰

## 7. 行动步骤

建议按这个顺序执行：

1. 先跑 `reload_basic_single_script`
2. 再跑 `reload_all_scripts`
3. 再跑 `reload_missing_file` / `reload_syntax_error` / `reload_runtime_error`
4. 然后做 `reload_base_shared_module`
5. 最后做现成 E2E 主流程回归：`resetdb` + `run-integration-tests.ps1 -NoCoverage`

## 8. 验收标准

本计划通过条件：

- 所有 P0 场景均在真实服务端 E2E 下通过
- 服务端日志中无未登记 `ERROR/FATAL`
- 真实客户端输出与脚本行为一致
- 失败场景保留旧版本，而非直接崩溃
- 任何临时脚本都在测试结束后清理，不留在工作区

## 9. 执行记录

- [ ] 计划已确认：E2E 目标不是单测 API，而是“真实服务器重载后业务仍稳定”
- [ ] 需要补充 `tests/integration/scenarios/reload/*.sce` 以及临时脚本目录
- [ ] 需要串联 `run-integration-tests.ps1` 作为验收入口
- [ ] 需保留服务端日志对照，明确脚本错误 / 重载成功 / 保留旧版本的证据

## 10. 并发与资源隔离设计（新增）

### 10.1 配置文件隔离（每轮动态生成）

- **不共用 `tests/integration/config.ini`**：该文件仅作模板，每轮运行由脚本动态生成独立副本
- **动态生成流程**：
  1. 读取模板 `tests/integration/config.ini.template`
  2. 替换占位符：`{PORT}` → 动态分配端口、`{DB_FILE}` → 独立数据库文件、`{LOG_DIR}` → 独立日志目录、`{SCRIPT_ROOT}` → 独立脚本根目录
  3. 写入临时配置：`%TEMP%\e2e_reload_<run_id>\config.ini`
  4. 启动服务器时指定 `--config %TEMP%\e2e_reload_<run_id>\config.ini`
- **端口动态分配**：从 6611 起始，每轮 +1，避免与并发的其他 E2E 跑冲突
- **数据库隔离**：每轮独立 `e2e_reload_<run_id>.db`，`-ResetDb` 只删当前轮的库文件
- **日志隔离**：`--log-run-id reload_<run_id>` + 独立日志目录 `%TEMP%\e2e_reload_<run_id>\log\`
- **脚本根目录隔离**：每轮复制一份 `reload_scripts/` 到 `%TEMP%\e2e_reload_<run_id>\scripts\`，服务器加载时指向该目录

### 10.2 运行时资源清单（每轮独享）

| 资源 | 隔离方式 | 清理时机 |
|------|----------|----------|
| 配置文件 | `%TEMP%\e2e_reload_<run_id>\config.ini` | 运行结束自动删临时目录 |
| 监听端口 | 动态分配 6611+N | 进程退出自动释放 |
| 数据库文件 | `e2e_reload_<run_id>.db` | 运行结束删除 |
| 日志目录 | `%TEMP%\e2e_reload_<run_id>\log\` | 运行结束删除 |
| 脚本根目录 | `%TEMP%\e2e_reload_<run_id>\scripts\` | 运行结束删除 |
| 临时脚本修改 | 仅在上述脚本根目录下操作 | 随脚本根目录一起删除 |

### 10.2 脚本目录隔离（每轮私有副本）

- **模板源目录**：`tests/integration/reload_scripts/`（只读，存放场景模板脚本）
- **运行时私有副本**：每轮启动时复制到 `%TEMP%\e2e_reload_<run_id>\scripts\`
- **子目录结构**（复制后）：
  - `scripts/npc/` — NPC 重载脚本
  - `scripts/item/` — Item 重载脚本
  - `scripts/room/` — Room 重载脚本
  - `scripts/base/` — 共享 base 模块副本
- **禁止写入**：`lua-default/`、`lua-sample/`、`data/`、`tests/integration/reload_scripts/`（模板目录只读）
- **修改仅在私有副本**：剧本中 `cmd echo ... > %SCRIPT_ROOT%/npc/xxx.lua` 只改私有副本
- **清理机制**：运行结束删除整个 `%TEMP%\e2e_reload_<run_id>\` 树，无需单独 `teardown.sce`

### 10.3 并发执行约束

- **单道串行**：同一账号/角色下的 reload 场景必须串行（避免同一实例被并发重载导致状态竞态）
- **跨道并行**：不同 lane（不同账号）可并行跑各自的 reload 场景，互不干扰
- **共享 base 修改串行化**：涉及 `base/` 共享模块修改的场景，必须在全局串行区执行（不能与其他 lane 并行），防止 `package.loaded` 污染
- `reload all` 互斥：全量重载期间禁止其他 lane 发起 reload，建议在 `lane_reload.sce` 中统一编排

### 10.4 状态复位与幂等

- **重载前快照**：每个场景开始前记录关键对象状态（hp、位置、背包、技能等级）
- **重载后校验**：reload 完成后对比快照，确认非预期字段未变
- **幂等重载**：同一脚本连续 reload 两次，第二次应为 no-op，不产生副作用

### 10.5 服务端日志关联

- 每个 reload 场景在剧本中记录 `--log-run-id` 对应的日志文件名
- 失败时直接定位到 `tests/integration/log/M-D-e2e-<id>.txt` 对应区间
- 服务端 ERROR/FATAL 白名单需在 `tests/integration/log-allowlist.txt` 登记 reload 预期错误

## 11. 与现有 E2E 道的兼容

| 现有道 | 兼容性说明 |
|--------|------------|
| L1 装备/采集 | reload 场景不占用木箱/药箱，独立脚本目录 |
| L2 技能/内功 | 可复用 L2 账号做 NPC/item reload 验证，但需串行 |
| L3 战斗/副本 | reload 场景不进入副本，避免副本实例状态复杂化 |
| L4 社交/交易 | 可复用 L4 账号做 room reload，但需独立房间 |
| L5/L6 配对互动 | reload 场景不参与配对，避免 peer 状态串扰 |
| L7 观战 | reload 场景不依赖观战信号 |
| L8 山贼试炼 | reload 场景不依赖 L3/L8 握手 |

**原则**：reload 专用道（建议新增 `lane_reload.sce`）独立运行，不与现有 8 道并发；若需复用账号，必须在现有道收尾后、下一轮开始前插入。

## 12. 资源清单（Checklist）

- [ ] `tests/integration/config.ini.template` 模板文件已建（含 `{PORT}` `{DB_FILE}` `{LOG_DIR}` `{SCRIPT_ROOT}` 占位符）
- [ ] 启动脚本能动态生成 `%TEMP%\e2e_reload_<run_id>\config.ini` 并传给服务器 `--config`
- [ ] 启动脚本能动态分配端口（6611+N）、生成独立数据库文件、独立日志目录、复制脚本根目录
- [ ] `tests/integration/reload_scripts/` 模板目录结构已建（npc/item/room/base）
- [ ] `tests/integration/scenarios/reload/*.sce` 使用环境变量 `%SCRIPT_ROOT%` `%PORT%` `%DB_FILE%` 等，不硬编码路径
- [ ] `tests/integration/log-allowlist.txt` 已登记 reload 预期 ERROR 签名
- [ ] 运行结束自动清理 `%TEMP%\e2e_reload_<run_id>\` 整个临时树
- [ ] `run-integration-tests.ps1 -Workers 1` 可单独跑 reload 道（单道串行）
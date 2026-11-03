# 玩家收徒集成测试计划（E2E）

## 目标

在真实服务器 + 多账号并行 CLI 场景下，验证“玩家作为师父收徒”的完整业务链路：
- 玩家师父 `/recruit` 玩家徒弟 → 师徒关系建立、`family/user=1` 写入、双方私发回执到达
- 徒弟 `/apprentice` 玩家师父 → 同一链路反向验证
- NPC 师父收徒路径不写 `family/user`（回归保护）
- 收徒经验 `/rexp`、开革 `/expell`、传授 `/teach` 相关命令分支在玩家师父场景下的表现

## 现有集成测试基线

| 道 | 现有覆盖 | 缺口 |
|---|---|---|
| L1 (lane_foot) | `recruit_teach_guard.sce`：只验「无收徒经验 / 开革非玩家 / 无伴侣」三个拒绝分支 | 无玩家师父正向收徒流程 |
| L2 (lane_skill) | `learn_basic.sce`：拜师 NPC（青云子）→ 请教基础技能 | 仅 NPC 师父路径 |
| L4 (lane_social) | `common/apprentice.sce`：同上，拜师 NPC | 仅 NPC 师父路径 |
| L5/L6 (互动道) | 结婚/组队/PK/摆摊等玩家间互动 | 无师徒互动 |

## 新增集成测试场景设计

### 场景 A：玩家师父主动收徒（`/recruit`）
- **前置**：两个账号（师父、徒弟）均已注册、登录、在同一房间（青云山山脚）
- **师父前置**：已有门派、代数 ≥1、`recruit_exp > 0`（可通过巫师命令或前置剧本给满）
- **步骤**：
  1. 师父 `/t <徒弟账号ID>` → `/recruit`
  2. 徒弟收到私发「如果你愿意拜xxx为师父，请与其交谈」
  3. 徒弟 `/t <师父账号ID>` → `/apprentice`
  4. 双方收到私发确认，师徒关系建立
- **断言**：
  - 徒弟 `family/master_id` = 师父账号ID
  - 徒弟 `family/family_name` = 师父门派
  - 徒弟 `family/generation` = 师父代数 + 1
  - 徒弟 `family/user` = 1
  - 师父 `/rexp` 能查到该徒弟贡献经验

### 场景 B：玩家徒弟主动拜师（`/apprentice`）
- **前置**：同上，但徒弟先发起
- **步骤**：
  1. 徒弟 `/t <师父账号ID>` → `/apprentice`
  2. 师父收到私发「如果你愿意收xxx为弟子，请与其交谈」
  3. 师父 `/t <徒弟账号ID>` → `/recruit`
  4. 双方确认建立关系
- **断言**：同场景 A

### 场景 C：NPC 师父收徒（回归）
- **复用**：`common/apprentice.sce` 现有流程（拜师青云子）
- **断言**：徒弟 `family/user` = 0 或缺失

### 场景 D：收徒经验查询 `/rexp`（玩家师父）
- **前置**：场景 A 完成，师父有至少 1 个玩家徒弟
- **步骤**：师父 `/rexp`
- **断言**：输出包含徒弟名、账号ID、贡献经验、弟子标记

### 场景 E：开革玩家徒弟 `/expell`
- **前置**：场景 A 完成
- **步骤**：师父 `/t <徒弟账号ID>` → `/expell`
- **断言**：双方收到私发「你被开革出师门了」，徒弟家族关系清空

### 场景 F：传授 `/teach`（需伴侣，作为扩展验证）
- **前置**：师父有伴侣（结婚流程），伴侣在同房间
- **步骤**：师父 `/teach` 列表 → `/teach <技能>`
- **断言**：伴侣技能提升，私发回执到达

## 实施方案

### 1. 新增 provision 片段（给师父账号准备收徒资格）
```
tests/integration/scenarios/provision/master_recruit_prep.sce
```
- 登录师父账号
- 巫师命令或脚本给 `recruit_exp` 赋值（如 `call set_recruit_exp 100`）
- 买基础装备/药品（复用现有 provision 逻辑）

### 2. 新增功能片段
```
tests/integration/scenarios/social/player_recruit.sce      # 场景 A
tests/integration/scenarios/social/player_apprentice.sce   # 场景 B
tests/integration/scenarios/social/player_rexp.sce         # 场景 D
tests/integration/scenarios/social/player_expell.sce       # 场景 E
```

### 3. 接入现有道
- **L4 (lane_social)**：在 `common/apprentice.sce` 之后、或 `recruit_teach_guard.sce` 替换位置，include 新片段
  - 理由：L4 已有社交补给、账号命令、睡觉观察等前置，且在山脚与半山腰往返，适合做师徒正向流程
- **L5/L6 (互动道)**：可选，作为玩家间师徒互动的额外验证（需双账号配对）

### 4. 变量注入
- `run-integration-tests.ps1` 注册阶段已为各道生成 `vars`（含 `acct`、`name`、`pass`）
- 新片段直接引用 `$acct`、`$name`、`$peer_acct` 等现有变量，无需额外注入

## 并行/隔离纪律（§8.4 / §8.6）

- 师徒关系仅在参与的两个账号间建立，不涉及稀缺资源（木箱、树箱、排行榜、蛇胆等）
- 所有回执为 `tell_object`/`notify_fail` 私发，天然免疫同房间串扰
- `/t` 定向统一用数字账号 ID（`$peer_acct`），跨房间有效
- 片段内若改 `/t` 目标，块末必须 `/t 0` 复位

## 执行与验收

```powershell
# 完整跑（含覆盖率）
.\run-integration-tests.ps1

# 快速调试单道（L4 社交道）
.\run-integration-tests.ps1 -NoCoverage -Workers 4
# 仅跑 L4：需临时修改脚本只启用 lane_social.sce，或手工 CLI 跑片段
```

**验收标准**：
- 场景 A/B/C/D/E 全绿（F 可选，依赖结婚前置）
- 覆盖率 `delta ≥ 0`（对比 `coverage_history/summary-e2e-sample.txt`）
- 服务端日志无未登记 ERROR/FATAL
- 新增片段不超绝对预算（420s），不落关键路径（L3 山顶战斗尾）

## 后续扩展（不在本计划）

- 徒弟出师/背叛/防沉迷分支
- 师徒跨地图传送/组队经验加成
- 巫师批量管理师徒关系命令覆盖

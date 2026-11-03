# 计划：sample 世界新增包子与蛇胆毒药

> 说明：本文件仅为实施计划，不包含任何实际代码改动。目标是参考 c:\jianghu-dataset / lua-sample 现有用法，把食物、毒药、商贩货架与山顶怪物改成一条可落地的方案，并同步补上测试计划。
>
> 状态：已实施——山顶改刷青蛇（`sample_pheasant` 删除），新增包子与蛇胆（`sample_baozi` / `sample_shedan` / `snake_poison`）；断肠散连同其状态脚本一并删除，蛇胆平替其商贩货位与全部数值；青蛇 `die` 把蛇胆丢在脚下由玩家自取；剧本（L3 采购与山顶链、L4 毒药链）与单测（`test_sample_food_poison.cpp`）同步补齐。

## 1. 目标

1. 在 sample 世界中新增“包子”作为食物，使用 `food_base.lua` 的通用逻辑。
2. 在山脚商贩处售卖该食物，复用现有 vendor NPC 的货架机制。
3. 把山顶练级怪从“山鸡”改为“青蛇”，并让其掉落“蛇胆”。
4. 将“蛇胆”做成毒药物品，复用 `poison_base.lua` 的通用逻辑。
5. 针对上述改动补齐可执行的脚本/剧本测试与回归检查清单。

## 2. 参考用法（基于当前仓库现状）

### 2.1 食物通用逻辑

现有 sample 世界的食物脚本大多遵循两种模式：

- 直接按 `type = "food"` 定义，使用 `do_use` 或直接复用 `food_base.do_use`。
- 通用逻辑放在 `lua-default/item/food_base.lua`，sample 侧 item 脚本通过 `require("item.food_base")` 复用。

现状样例：

- `lua-sample/item/sample_heal.lua`：定义“金创药”，使用 `type = "food"`，`Can_Use` 与 `do_use` 自己实现。
- `lua-default/item/food_base.lua`：提供统一的“吃东西 / hp/mp 恢复 / finish_eat 销毁 / 物品留存”逻辑，适合包子这类普通食物。

### 2.2 毒药通用逻辑

毒药类脚本通常执行使用路径：

- 物品脚本 `type = "food"` 或 `type = "poison"`，并声明 `poison_type`。
- 通过 `require("item.poison_base")` 复用通用逻辑。
- `do_look` / `handle_action` / `do_mo` 等由 `poison_base` 提供。

现状样例：

- `lua-sample/item/sample_poison.lua`：演示“断肠散”直接伤害 + 持续毒性状态。
- `lua-dataset/item/food/shedugao.lua`：演示 `poison_base` 的实际使用方式，属于“蛇毒膏”这类“涂毒到武器”的物品。

### 2.3 商贩货架

现有 sample 商贩的货架维护在 `lua-sample/npc/sample_vendor.lua`：

- `npc.type = "vendor"`
- `self:carry_good("<item_id>", <qty>)` 上架商品
- `do_talk` / `handle_action` 中保留对 C++ vendor 基类 `base_handle_action` 的显式回退

按照当前设计，新增商品直接在 `sample_vendor.lua` 的货架列表中追加即可，且不需要新建 vendor NPC。

### 2.4 山顶怪物和击杀奖励

现有山顶练级怪物定义在 `lua-sample/npc/sample_pheasant.lua` 和 `lua-sample/room/sample_mountain_top.lua`：

- `sample_pheasant` 作为 `xp_bonus = 1` 的低级野兽
- 山顶房间通过 `self:add_npc("sample_pheasant")` 生成
- `lua-sample/data/hooks.lua` 中 `hooks.kill_bonus` 处理击杀奖励，当前写死“山鸡”收益 10000 经验

因此“山鸡改成青蛇”这部分，最稳妥的方案是：

- 让山顶的 NPC 改名为“青蛇”或新增 `sample_qingshe.lua`
- 保持其 `xp_bonus` / 练级语义不变；若需要继续保留经验收益，仍由 `hooks.kill_bonus` 做奖励分流
- 额外增设“青蛇掉蛇胆”的掉落/奖励逻辑，按当前 sample 世界的脚本钩子模式实现，不直接动引擎 C++ 逻辑

## 3. 实施方案（计划）

### 3.1 新增包子

1. 在 `lua-sample/item/` 下新增 `sample_baozi.lua`。
2. 文件结构遵循 dataset 的常见做法：
   - `return { type = "food", ... }`
   - `create = function(self, flag) ... end`
   - `Can_Use = function(self) return 1 end`
   - `do_use = food_base.do_use`
3. 物品字段建议：
   - `self:set_name("包子")`
   - `self:set("unit", "个")`
   - `self:set("long", "一个热气腾腾的包子。")`
   - `self:set("value", 200)` 或按样本世界物价体系调
   - `self:set("food_remaining", 1)` / `self:set("food_supply", 15)`，按 food_base 的约定保留可复用字段
4. 若需要更贴合 dataset 风格，也可以保留 `eat_msg` 已有的 `message_vision` 文案约定，让吃包子时视觉反馈一致。

### 3.2 在商贩处售卖

1. 修改 `lua-sample/npc/sample_vendor.lua` 中的货架列表。
2. 追加 `self:carry_good("sample_baozi", 20)`，确保其可在山脚杂货铺中出现。
3. 以同样机制保留“金创药 / 断肠散 / 传送符 / 茶壶”等样例商品的兼容性。
4. 验证买卖命令 `/list` / `/buy` / `/sell` 仍然从 vendor 的 base_handle_action 流程走通。

### 3.3 山顶怪物改为青蛇并掉蛇胆

1. 在 `lua-sample/npc/` 下新增/替换对应怪物脚本：如 `sample_qingshe.lua`，取代现在的 `sample_pheasant.lua`。
2. 配置字段：
   - `self:set_name("青蛇", "qing she")`
   - `self:set("race", "野兽")`
   - `self:set("long", "一条青色的蛇，眼神锐利，头部微微抬起。")`
   - 保持 `level` / `combat_exp` / `max_hp` / `xp_bonus` 与练级流程兼容
   - 使用 `self:set_beast_verbs({ "poke" })` 或与现有动物模板一致的攻击动作白名单
3. 在 `lua-sample/room/sample_mountain_top.lua` 中把 `self:add_npc("sample_pheasant")` 替换为 `self:add_npc("sample_qingshe")`。
4. 在 `lua-sample/data/hooks.lua` 中增加“击杀青蛇掉落蛇胆”的奖励挂点逻辑：
   - 若 `victim` 是青蛇，则返回包含蛇胆奖励对象或走当前 sample 世界中最接近的掉落动作。
   - 若项目不支持直接投放物品，至少应保留 `hurt/damage` / `kill_bonus` 的脚本入口，确保该掉落逻辑集中在 sample 侧，不改 C++ 引擎核心逻辑。

### 3.4 蛇胆做毒药

1. 在 `lua-sample/item/` 下新增 `sample_shedan.lua`。
2. 参考 `shedugao.lua` / `poison_base.lua` 的设计：
   - `type = "food"` 或 `type = "poison"` 均可，按照当前通用毒药约定选用稳定方案
   - `require("item.poison_base")`
   - `create` 中声明 `self:set("poison_type", "snake_poison")`
   - `do_look` / `handle_action` / `do_mo` 复用 `poison_base` 的实现
3. 物品字段建议：
   - `self:set_name("蛇胆")`
   - `self:set("unit", "颗")`
   - `self:set("long", "一颗从青蛇体内取出的蛇胆，既可入药，也能制成毒药。")`
   - `self:set("value", 0)` 如果作为“材料/毒药”而非可直接卖卖品
4. 若需要“毒药”语义更强，则增加 `Can_Use` / `do_use` 的自定义路径：
   - 服下立即中毒
   - 或渲染成“涂毒到武器”的路径，复用 `poison_base.do_mo`

## 4. 代码范围与文件清单

计划会修改（但本次只保存计划，不执行）以下文件：

- `lua-sample/item/sample_baozi.lua`（新增）
- `lua-sample/npc/sample_vendor.lua`（货架增加）
- `lua-sample/npc/sample_pheasant.lua`（替换为青蛇脚本或新建 `sample_qingshe.lua`）
- `lua-sample/room/sample_mountain_top.lua`（怪物换名 / 生成逻辑）
- `lua-sample/item/sample_shedan.lua`（新增）
- `lua-sample/data/hooks.lua`（击杀掉落/奖励挂点）

## 5. 测试计划

### 5.1 目标测试维度

本次改动涉及三类能力：

1. 食物新物品是否能正确进入商贩货架、被买到、使用并产生恢复效果。
2. 山顶怪物是否被成功替换，并在击杀后给奖励/掉落。
3. 蛇胆是否作为毒药物品能被识别、使用、涂抹到武器并触发对应毒药状态。

### 5.2 推荐测试方法

#### A. 单元/脚本层测试

- 直接用 `lua-default/item/food_base.lua` 与 `lua-default/item/poison_base.lua` 的既有接口测试断言，确保：
  - `food_base.do_use` 能在 `food_remaining > 0` 时恢复 hp/mp
  - `poison_base.do_mo` 能在满足 mp / 武器条件时设置 `poison/modu`
  - `poison_base.handle_action` 对 `use` / `tumo` 分支没有回退错误
- 对新增 sample 物品脚本单独校验：
  - `sample_baozi` 类型为 `food`
  - `sample_shedan` 具备毒药字段/行为

#### B. 集成剧本测试

建议新增 / 更新 integration 场景，按一条主线覆盖：

1. 登录山脚
2. 访问商人，执行 `/list` 及 `/buy sample_baozi`
3. 断言：你从杂货老板那里买下了包子
4. 使用包子：`cmd use 包子`（或对应客户端命令）
5. 断言：输出里出现 `hp+` / `mp+` 或等价回执文案
6. 前往山顶，清掉青蛇
7. 断言：收到“蛇胆”这一物品或掉落/奖励回执
8. 对“蛇胆”执行 `use` / `tumo`，断言毒药状态生效

### 5.3 失败判定标准

- 商贩没有售卖包子：测试失败。
- 包子不能被使用或没有恢复效果：测试失败。
- 青蛇击杀后没有掉落/奖励：测试失败。
- 蛇胆没有进入毒药流程或状态脚本不生效：测试失败。
- 任何一条 `wait text` / `wait menu` 断言超时：测试失败。

### 5.4 验收顺序

1. 先跑样本脚本的语法检查：`jianghu.exe --check-lua` 或等价在 repo 中的 Lua 校验脚本。
2. 再跑 integration 相关剧本（山脚商贩 + 山顶掉落 + 食物/毒药使用链）。
3. 最后跑 `run-coverage.ps1` 作为总回归门槛，确保新增对象没有破坏 sample 世界其他路径。

## 6. 风险与注意事项

1. `food_base.lua` 里 `if addhp == 0` 的判断要格外注意 Lua 中 `0` 也是真值，修改时必须显式用 `== 0` / `~= 0` 处理，不要沿用 C/C++ 的惯用写法。
2. 需要避免将“毒药”“食物”用成同一套错误脚本：`poison_base` 的 `use` / `tumo` 逻辑与 `food_base` 的恢复逻辑不是同一层。
3. 山顶怪物和商贩几个脚本均属于 sample 世界的示例构件，不要在 `lua-default` 中硬塞 test-only 逻辑；这类改动应保持在 `lua-sample` 与 `tests/integration` 之间。
4. 如果最终确认项目不支持直接“击杀掉落物品”，则掉落逻辑必须通过 `hooks.kill_bonus` 或更高层的 sample-only 挂点实现，不得改引擎生产代码作为测试补丁。

## 7. 结论

本次需求可分成两部分：

- 食物/商贩侧：新增包子 + 商贩售卖 + 使用链路覆盖
- 草木/毒药侧：青蛇 + 蛇胆 + 毒药状态链路覆盖

两者都可以用 sample 世界已有的 `food_base` / `poison_base` / vendor / hooks 模式完成，最妥当的落地方式是“只改 sample 世界脚本 + integration 剧本验证”，不触碰 `lua-default` 生产逻辑与 C++ 引擎。

这份计划也满足了“先设计、再测试、最后才实施”的顺序要求；当前文件仅保留计划，不真的执行改动。

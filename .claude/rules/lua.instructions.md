---
name: lua coding
description: 新建或编辑lua代码时的一些注意事项
applyTo: '**/*.lua'
---

# Lua 语义陷阱（最容易踩的坑）

## 1. `if 0 then` 在 Lua 中为真！

**Lua 中只有 `false` 和 `nil` 是假，`0` 是真**！而 C++ 中 `if(0)` 为假。

```lua
-- 错误（C++ 习惯，Lua 中 0 为真，会误判）
if me:query_temp("sucked") then
if me:Query(IS_LIVING) then        -- 恒 true，else 永不进入
if not me:Query(IS_LIVING) then    -- not 0 恒 false，分支永不进入

-- 正确
if me:query_temp("sucked") ~= 0 then
if me:Query(IS_LIVING) == 0 then   -- 数值为 0（false 语义）
if me:Query(IS_LIVING) ~= 0 then   -- 数值非 0（true 语义）
```
> **教材实物案例（2026-10-31 修复）**：`lua-default/item/liquid_base.lua` 的 `do_use`
> 曾写 `if self:query("liquid/drink_func") then`，而 `do_fill` 用 `drink_func = 0`
> 表示「无自定义效果」——`if 0 then` 恒真使 fill 过的容器永远短路，alcohol/xx_poison
> 醉酒毒酒分支结构性不可达（E2E 茶棚片段首次上马即撞上）。判定必须显式
> `~= 0`：nil 与 0 都走通用分支。

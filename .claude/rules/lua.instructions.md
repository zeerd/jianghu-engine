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

判定必须显式 `~= 0`：nil 与 0 都走通用分支。

## 2. 随机数：全局 `random(n)` 是 0 基，`math.random(n)` 是 1 基

引擎绑定的全局 `random(n)` 取 **[0, n-1]**（n ≤ 0 时返回 0，实参必须是整数）；
Lua 标准库 `math.random(n)` 取 **[1, n]**（单参 0/负数非法）。两者相差 1，方向写
反就取到 nil，或让概率分支恒假/恒真：

```lua
local name = pool[random(#pool) + 1]   -- 全局 random：0 基，必须 +1
local name = pool[math.random(#pool)]  -- math.random：1 基，不能 +1
local dmg  = base + random(mp // 2)    -- 实参用 // 或 math.floor(...) 包住
if math.random(3) == 1 then ... end    -- math.random >= 1，写 == 0 恒假
```

- 新脚本一律用 `math.random`；全局 `random` 已废弃，`lua-default` / `lua-sample`
  里不许出现（`--check-lua` 会拦），仅存量数据集还在用。
- 表按 `for i = 0, n - 1` 建成 0 基时，配套用 0 基取值（`random(n)` 或
  `math.random(0, n - 1)`）；同一张表两种下标基线不许混用。

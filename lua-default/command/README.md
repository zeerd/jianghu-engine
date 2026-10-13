# Lua 命令模块（一命令一文件）

C++ 三张命令表（标准 / 用户 / 巫师）未命中后，分发链落到全局 `commands` 表；
表内未命中的命令名由 C++ 装入的分发器按名 `require("command.<命令名>")` 懒加载。
**新增命令 = 在本目录放一个文件**，不需要改动任何共享文件，也不需要 C++ 注册——
目录本身就是命令清单。

## 文件形态

一个命令一个文件，文件名即命令名，返回模块表，入口函数键名与文件名相同：

```lua
-- command/<命令名>.lua
local M = {}

function M.<命令名>(me, target, arg)
    -- me     发令者（C++ userdata）
    -- target 目标对象 id（数字，来自命令解析）
    -- arg    参数串
    -- 返回 1 = 已处理；0 = 失败（失败前自行 notify_fail）；
    --      无返回视为 1；返回 -1 = 交回分发链（emote / 未知的命令）
    return 1
end

return M
```

- **文件名 = 命令名**（`makelove.lua` → `makelove` 命令），`snake_case`。
- **入口键必须与文件名同名**，否则分发器报 `has no entry function`。
- 命令名只允许字母 / 数字 / 下划线；含 `.` 或 `/` 的输入被拒（防 require 越界模块路径）。
- 权限由脚本自检：C++ 三张命令表优先于本目录，返回 -1 可把命令让回分发链。

## 加载与生效

- 分发器（`commands` 表的 `__index`）由 C++ 在开机时 `lua_commands_install` 装入，
  脚本无需建表或登记；`commands` 表本体不依赖任何 `data/commands.lua` 文件。
- 命令文件缺席 = 静默回落（表情 / 拼错命令名是常态），不记日志。
- 命令文件存在但加载失败 / 入口缺失 = 记一次 `log_error` 并缓存失败，不刷屏。
- **新文件放进本目录后需 `reload all`**（分发器缓存随之清空）；改坏的文件修好后
  同样靠 `reload all` 撤销失败记录。

## 两个必踩的坑

### 1. call_out 必须传函数值

命令模块内挂延迟回调时，宿主可能是**玩家**（没有脚本表，按名解析取不到）或
**NPC**（按名解析查到的是 NPC 自己的脚本表，同样取不到本模块的回调）。两种情况
都只有函数值稳定：

```lua
-- ❌ 按名解析：玩家注册失败、NPC 查错表
me:call_out("makelove_over", 1, who_id)

-- ✅ 函数值：闭包直接持有本模块的回调
local function over(self, param1) ... end
me:call_out(over, 1, who:object_id())
```

分发期间 C++ 会把 `commands` 表登记为"当前脚本引用"，因此对玩家对象挂函数值回调可以注册成功。

### 2. 数值真值

`query` / `query_temp` / `combatd_query` 返回**数值**，而 Lua 里 `0` 是真值——
判存在必须显式 `== 0` / `~= 0`：

```lua
if env:query("sleep_room") == 0 then ... end   -- ✅
if not env:query("sleep_room") then ... end     -- ❌ 永不为真
```

`userp` / `wizardp` / `living` / `is_busy` 返回布尔，用 `not` 才对。

## 与 C++ 命令表的分工

留在 C++ 的是**引擎驱动**（命令解析、分发、`handle_action` 截获、频道 / emote）；
命令的**玩法内容**一律 Lua 化。判断标准同数据所有权：命令自己读写的 dbase / temp 键、
消息文案、流程分支都属于内容。

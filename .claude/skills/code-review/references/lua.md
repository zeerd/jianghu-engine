# Lua 评审参考

改动落在 `lua-default/`、`lua-sample/`、`tests/` 的 Lua 脚本时读本文件。依据
分层：**仓库规则与 `.stylua.toml` 优先，风格指南只兜底**。原技能默认的 Roblox
风格指南在本工程有两处硬冲突：**camelCase 命名不
适用**（本工程 Lua 统一 `snake_case`），if-expression/Roblox API 等 Luau 内容
不适用（本工程是 plain Lua 5.5）。

## 确立运行时与宿主

从工程配置读实际运行时：**Lua 5.5**（`3rd/include/lua55`、`lua55.lib`），经
`CLuaEngine` 嵌入 C++ 引擎，绑定层由 `lua_bind_init()` 提供。不要从 `.lua`
扩展名或本机默认 `lua` 二进制猜版本；不要建议 LuaJIT/Luau/Lua 5.1 特有语法
（5.1 缺失的标准库在 5.5 都有，如 `table.pack`/`table.unpack`）。语义争议查
[Lua 5.5 手册](https://www.lua.org/manual/5.5/manual.html)对应章节。

宿主边界：

- `lua-default/` 是**生产代码**（默认脚本），`lua-sample/` 是**教材**
  （演示最少代码最多功能）——两者都不是测试夹具，见下方专项检查。
- C++↔Lua 边界：错误穿越 C++ 栈的行为以引擎的 protected call 与清理路径为准，
  不臆断 Lua error 会执行 C++ 析构；GC 不是外部资源的及时释放保证，看宿主
  的显式清理契约。

## 仓库命名规则（覆盖 Roblox 指南的 Naming 节）

评审时最常要对表的几条：

- 文件/模块名、函数、局部变量一律 `snake_case`，与对象 `base_name` 一致；
  **报「应改 camelCase/itemCount」在本工程是错误结论**。
- 模块表固定 `M` 或对象类型名；常量 `UPPER_SNAKE`；私有加前导 `_`。
- 对象脚本文件名即游戏数据键，必须与 `base_name` 一致。
- 与 C++ 绑定表键同名的函数保持原名（改名会断绑定）。

格式以 `.stylua.toml`（4 空格缩进）为准，不逐条报格式——统一交给
`style.ps1`。

## 审查问题表

| 领域 | 何时关注 | 报告前需要的证据 |
| --- | --- | --- |
| 真值与默认值 | `nil` vs `false`、`x or default`、and/or 条件 | 受支持的 false 值被替换或走错分支 |
| 表形状 | 稀疏数组、`#`、`ipairs`、遍历中删除 | 实际键形状与被跳过的元素/不稳定结果 |
| 作用域 | 意外全局、共享模块表、捕获的 upvalue | 另一次调用/协程观察到非预期共享状态 |
| 调用方式 | `:` vs `.`、隐式 self、多返回值、括号 | 被调方签名与实际传/丢弃的值 |
| 错误处理 | `pcall` 状态、`nil, err` 约定、清理、传播 | 失败被报成成功，或必需清理丢失 |
| 数值与字符串 | 大 ID 精度、强转、字节 vs 字符下标、模式 | 超出假定精度/编码/模式约束的受支持输入 |
| 宿主 API | 绑定方法、心跳计时、`start_busy`、私发/广播 | 宿主特定的执行上下文与失败路径 |
| C API | 栈平衡、注册表引用、跨原生帧的所有权 | 原生/VM 生命周期与具体栈违规 |

**`if 0 then` 为真**（本工程最高频陷阱）：
只有 `nil` 和 `false` 是假，`0`、空串是真。数值型标志必须显式 `== 0` /
`~= 0` 判定；`if obj:Query(KEY) then` 恒真、`not 0` 恒假就是缺陷。`or` 默认
值只有在契约要保留 false 时才是 bug。`#` 不是稀疏表的通用计数，`ipairs` 遇
缺即停，`pairs` 无序。多返回值经赋值/加括号/非末位会截断。

## 本工程专项检查

- **样本教材铁律**：`lua-sample/` 不得为测试目的加异常逻辑或脚手架；每个调用
  要符合事实逻辑、不为演示生硬拼接；改动是否该发生，理由必须是教学设计。
- **生产脚本铁律**：`lua-default/` 的改动理由必须是机制/契约/世界设定；注释
  出现测试计划、E2E 剧本、覆盖率、验收清单、文档编号——发现。
- **为测试改生产**：遇测试失败先看测试侧（剧本、补给、`-ResetDb`、config），
  在 `src/` 或 `lua-default/` 里塞测试特判/硬编码键名保值是发现。
- **`style.ps1` 前的 BOM**：Lua 文件 UTF-8 无 BOM（`Remove-LuaBom.ps1` 纳管
  三个目录）。
- **绑定完整性**：新增脚本函数若要给 C++ 调用，核对绑定表；`--check-lua` 静态
  检查（require 闭包、未绑定全局/方法）是底线，其盲区见
  `src/lua/LuaSyntaxChecker.cpp` 文件头，不能拿工具绿灯当脚本正确的判定。

## 聚焦验证

只用仓库手段，从仓库根执行：

```powershell
# Lua 语法/绑定静态检查：jianghu.exe 是 GUI 程序，调用方式与 cwd 约束
# 必须照 copilot-instructions.md 的「正确的调用方法」，没有输出即失败
.\style.ps1                  # stylua（及 clang-format）格式自检
.\run-coverage.ps1           # 含 lua-tests（tests/unit/lua-tests）的唯一认可途径
```

交互行为以手工交互或 `.\run-integration-tests.ps1` 为准（用户要求或剧本需要
时才跑）。静态检查不能发现表形状、多返回值、宿主生命周期问题；从未运行的检查
不得声称通过。

## 主要来源

- [Lua 5.5 reference manual](https://www.lua.org/manual/5.5/manual.html)：
  语义权威。
- [Roblox Lua Style Guide](https://roblox.github.io/lua-style-guide/)：仅在
  仓库与本文件都沉默处参考其通用建议；Naming、if-expression、服务 API、
  缩进/列宽各节不适用。

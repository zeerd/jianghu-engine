# C++ 评审参考

改动落在 `src/`、`tests/`、`cli/` 时读本文件。依据分层：**仓库规则与
`.clang-format` 优先，Google C++ Style Guide 只兜底**；指南只
用于仓库沉默处，且是编码标准不是语言要求，合法代码不因违反指南被判「未定义/
损坏」。

## 确立构建契约

从最近的 `.vcxproj` / `.sln` 读实际设置：C++17（`stdcpp17`）、MSVC、
**默认 x64**（Win32 仍可构建，但不是默认平台）、`/source-charset:utf-8`、Level3 警告。
区分 C++ 源与生成头、区分 `src/`（生产）、`tests/`（Google Test）、`cli/`
（独立工具）三个面的规则差异。版本敏感的语言断言查 C++17 标准与 MSVC 文档，
不要求 C++17 目标使用 C++20 设施。

## 仓库命名规则（覆盖 Google 指南的 Naming 节）

评审时最常要对表的几条：

- 函数、方法、变量、参数一律 `snake_case`；**Google 指南的函数 PascalCase 在
  本工程不适用**，反报才是错的。
- 类 `PascalCase` 且带 `C` 前缀，struct 不带前缀；成员 `m_ + snake_case`（不是
  尾下划线）；静态 `s_`；全局 `g_`；常量/宏 `UPPER_SNAKE`（不是 `kMixedCase`）。
- 枚举暴露给 Lua 的成员是协议，必须 `UPPER_SNAKE` 且不可改名。
- 被修改的存量符号，改到即改对，不为一致性保留旧风格。

格式以 `.clang-format`（Google 基底、4 空格、80 列、LF、`SortIncludes: false`）
为准，不逐条报格式——统一交给 `style.ps1`。结构硬指标：单行 ≤ 80 列、单文件
≤ 500 行、单函数 ≤ 50 行，超限是发现。

## 审查问题表

| 领域 | 何时关注 | 报告前需要的证据 |
| --- | --- | --- |
| 生命周期 | 返回引用/视图、临时量、lambda 捕获、异步回调 | 被借对象先于使用被析构或失效 |
| 所有权 | 裸/智能指针、转移、析构器、双重释放、错误路径清理 | 具体的所有权转移与泄漏/重复释放路径 |
| 容器 | 扩容、erase、迭代器/引用失效、越界、借用 span | 实际的容器操作与后续非法访问 |
| 对象语义 | 拷贝/移动、不变量、自赋值、切片、虚析构 | 某个受支持的操作或删除路径破坏契约 |
| 未定义行为 | 未初始化读、有符号溢出、非法移位、对齐、别名 | 可达的操作数值或存储用法，而非「看起来危险」 |
| 并发 | 数据竞争、锁序、锁内回调、条件变量谓词、原子量 | 可行的交错次序与被违反的同步/进展保证 |
| 异常 | 局部构造/更新、清理、`noexcept`、跨 API 边界抛出 | 可抛出的操作与随之而来的终止/泄漏/不变量破坏 |
| 兼容性 | 公开布局、虚表、导出签名、模板、序列化 | 受支持的源/二进制消费方或存量格式受影响 |

整数提升、有符号/无符号转换、尺寸运算对照实际边界判断；无符号回绕不是 UB，
可能是分配或下标契约被违反。moved-from 标准库对象一般有效但值不确定，先说清
操作与前置条件再报误用。

## 本工程专项检查

- **注释铁律**：`src/` 注释出现迁移记录、测试计划、E2E 剧本、覆盖率、验收
  清单、文档编号、被删代码的痕迹——发现；注释只能解释功能、接口、机制归属。
- **测试配套**：修改 `.cpp`/`.h` 未同步对应 `test_*.cpp`（或已有用例未更新），
  按 unit-test 规则报告；验证途径只有仓库根 `.\run-coverage.ps1`（Release）。
  新增测试的价值先过 test-audit 技能的编写门槛，不按行覆盖率凑数。
- **测试专用 seam**：生产代码里出现只有测试会用的导出、getter、reset、
  `#ifdef _TESTING`、注入参数——按 unit-test 的 seam 判据报告。
- **`char*` / `char foo[123]`**：新写或改写处优先评估 `std::string` 替代。
- **BOM**：新增与修改的源文件必须 UTF-8 无 BOM（`.ps1` 含非 ASCII 时例外，
  必须带 BOM）。
- **game_state 结构**：`game_state*.cc` 保持框架化，新主题必须落到独立
  `game_state_<主题>.cc`（cli-game-state 规则），不往 `game_state.cc` 里堆。

## 聚焦验证

只用仓库脚本，从仓库根执行；不自建 CMake，不跑 Debug：

```powershell
.\run-coverage.ps1            # 编译 + 单测 + 覆盖率（唯一认可途径）
.\run-cppcheck.ps1            # 结果在 bin\cppcheck.result.txt，有报必修
.\style.ps1                   # clang-format + stylua 自检
```

能跑则跑最小相关集（如 `--gtest_filter` 聚焦），如实记录命令与结果；从未运行
的检查不得声称通过。测试通过不反证未测路径安全。改过 C++ 后三件套缺一不可：
配套测试 + run-coverage + run-cppcheck。

## 主要来源

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)：
  仓库沉默处的兜底标准（Naming/Formatting 节除外，以仓库规则为准）。
- [ISO C++ standards](https://isocpp.org/std/the-standard)：语言断言的权威。
- [cppcheck](https://cppcheck.sourceforge.io/)：`run-cppcheck.ps1` 背后的
  静态分析器。

---
name: 'Unit Test'
description: 'Test functions after modified codes'
applyTo: '**/*.cpp,**/*.h'
---
在 `tests` 目录下使用 Google Test 框架创建单元测试。

`.\run-coverage.ps1`是唯一认可的编译和运行测试的方案和手段。其他方式的运行结果都是不被承认的。

**绝对禁止用 Debug 版运行测试用例。** 生产服务器以 `Release|Win32`（`/MT`、`NDEBUG`、`/O2`）构建；覆盖率也直接在 Release 上采集（Release 带 PDB，OpenCppCoverage 可正常做行映射），因此 `run-coverage.ps1` 从头到尾只有 Release 一个配置。

---

## 架构概览

测试框架采用 **静态库 + Mock 层** 架构：

```
jianghu_core.lib (静态库)
├── src/ 全部 .cpp (仅排除 main.cpp 与 db/Sql.cpp)
│    + 3rd/sqlite/sqlite3.c
│
├──→ jianghu.exe (服务器)
│    + main.cpp + db/Sql.cpp + jianghu_core.lib
│
└──→ tests.exe (测试)
     + mock_net.cpp (仅测试开关；Message.cpp 直接编译进库，不 mock)
     + mock_db.cpp (替代 db/Sql.cpp，并补 main.cpp 的 g_ini_data)
     + mock_game.cpp (空壳占位，无 mock 函数)
     + mock_itemmgr.cpp (替代物品管理器)
     + test_*.cpp (测试用例)
```

- **静态库**：仓库根的 `jianghu_core.vcxproj` 编译所有核心代码为 `jianghu_core.lib`
- **Mock 层**：`tests/unit/mock/` 下的 .cpp 通过链接时替换，为被排除出库的源码与外部依赖提供假实现
- **真实 Lua**：测试链接真实的 `lua55.lib`，Lua 引擎可正常初始化和执行脚本

## 单元测试规范

### 测试工程

- **引擎测试**：`tests/unit/tests.vcxproj` 是唯一的引擎测试工程；单元测试源码放在 `tests/unit/` 目录下，文件名格式 `test_<功能名>.cpp`。
- **CLI 测试**：`cli/tests/jianghu_cli_tests.vcxproj`（独立工程）；源码放在 `cli/tests/` 下，文件名格式 `<功能名>_test.cc`。
- `run-coverage.ps1` 分别构建并运行两个工程的测试，再以 `--input_coverage` 合并覆盖率后统一展示。

### 强制要求：修改必须有测试

**每次修改 `.cpp` 或 `.h` 文件后，必须同时提交对应的测试用例。** 具体规则：

1. **已有测试覆盖**：修改的函数/方法在 `test_*.cpp` 中已有测试用例 → 必须更新或补充该用例，确保新逻辑被验证。
2. **无测试覆盖**：修改的函数/方法在 `test_*.cpp` 中没有对应测试 → **必须新增测试用例**，覆盖修改涉及的分支和边界条件。
3. **新增文件**：新增 `.cpp` 文件 → 必须创建对应的 `test_<功能名>.cpp` 并添加到 `tests/unit/tests.vcxproj`。
4. **禁止无测试提交**：如果修改了业务代码但没有对应测试，视为未完成任务。
5. **协议函数必须有测试**：所有通讯协议相关的函数必须通过单元测试验证正确性。

### 框架：Google Test

测试使用 Google Test 框架（通过 NuGet 引入），提供以下核心宏：

| 宏 | 说明 |
|---|---|
| `TEST(Suite, Name)` | 定义一个测试用例，Suite 为模块名，Name 为用例名 |
| `EXPECT_TRUE(cond)` | 断言条件为真（失败后继续执行） |
| `EXPECT_FALSE(cond)` | 断言条件为假 |
| `EXPECT_EQ(a, b)` | 断言两个值相等（保留原始类型，支持指针） |
| `EXPECT_NE(a, b)` | 断言两个值不相等 |
| `EXPECT_STREQ(a, b)` | 断言 C 字符串相等 |
| `EXPECT_GT(a, b)` | 断言 a > b |
| `EXPECT_LT(a, b)` | 断言 a < b |
| `EXPECT_NE(p, nullptr)` | 断言指针非空 |
| `ASSERT_EQ(a, b)` | 值相等，失败立即终止当前用例 |
| `ASSERT_NE(p, nullptr)` | 指针非空，失败立即终止当前用例 |

- 不需要 `main()`，Google Test 自动提供。
- 不需要手动注册测试，`TEST()` 宏自动注册。
- **`EXPECT_*` 用于行为验证**（失败后继续执行，一次可见多个失败）；**`ASSERT_*` 用于前置条件检查**（如指针非空），失败即返回。

### 创建新测试

1. 在 `tests/` 下新建 `test_<功能名>.cpp`：
2. 编写测试并添加到 `tests.vcxproj` 的 `<ClCompile>` 列表中：

```cpp
// test_<功能名>.cpp - <功能描述>
//
#include "gtest/gtest.h"
#include "StringUtil.h"  // 可以直接 include 服务器头文件

TEST(XxxSuite, BasicOperation)
{
    EXPECT_EQ(valid_string("hello world"), 1);
}
```

3. 在 `tests.vcxproj` 的 `<ItemGroup>` 中添加：
   ```xml
   <ClCompile Include="test_<功能名>.cpp" />
   ```

### 夹具与测试命名

共享 setup/teardown 用 `TEST_F` + fixture。**`SetUp()` 中必须重置所有全局状态与 mock 计数器**（状态泄漏的表现：单独运行通过、批量运行失败），`TearDown()` 恢复被替换的全局指针：

```cpp
class SomeClassTest : public ::testing::Test {
protected:
    MockDependency mockDep;

    void SetUp() override {
        mockDep.reset();       // 重置计数器
        g_Dep = &mockDep;      // 注入 mock
    }

    void TearDown() override {
        g_Dep = nullptr;       // 恢复全局指针
    }
};

TEST_F(SomeClassTest, BasicBehavior)
{
    SomeClass obj;
    Result r = obj.DoSomething("input");
    EXPECT_EQ(r, ExpectedValue);
    EXPECT_EQ(mockDep.call_count, 1);
}
```

- 每个 `TEST_F` 只测试一个行为；测试名清晰表达 `<场景>_<期望结果>`；测试之间不许有依赖。
- 嵌套测试名用 `/` 分隔层级：`TEST(RoundTrips/EncodingRoundTrip, GbkToUtf8AndBack)` 运行时显示 `RoundTrips/EncodingRoundTrip.GbkToUtf8AndBack`。
- 测试文件顶部注释中给出构建和运行命令。

### 目录结构

```
tests/
├── tests.vcxproj       # VS 工程文件（唯一测试入口）
├── packages.config           # NuGet 包配置（Google Test）
├── pch.h / pch.cpp          # 预编译头（gtest/gtest.h）
├── mock/                     # Mock 层（链接时替换）
│   ├── mock_net.cpp          # 仅测试开关 g_test_suppress_output
│   ├── mock_db.cpp           # 替代被排除出库的 src/db/Sql.cpp；补 main.cpp 的 g_ini_data
│   ├── mock_game.cpp         # 空壳占位，当前无 mock 函数
│   ├── mock_itemmgr.cpp/.h   # CMockItemMgr：物品管理器假实现
│   └── mock_database.h       # MockDatabase / ThrowingDatabase（IDatabase 假实现）
├── lua-tests/                # Lua 夹具脚本
├── test_config.ini           # 测试配置（相对仓库根读取）
├── test_smoke.cpp            # 冒烟测试（验证框架链接正确）
└── test_<功能名>.cpp         # 其他测试文件
```

### Mock 层

Mock 文件通过 **链接时替换** 提供外部依赖的 mock 实现。当修改服务器代码导致 mock 函数签名变化时，必须同步更新对应的 mock 文件。

| Mock 文件 | 替代的服务器文件 | 主要 mock 函数 |
|---|---|---|
| `mock_net.cpp` | 无（`net/Message.cpp` 直接编译进库，不 mock） | `g_test_suppress_output`（抑制测试输出） |
| `mock_db.cpp` | `db/Sql.cpp` + `main.cpp`（`g_ini_data` 定义处） | `get_db_type`, `init_sql_server`, `restore_data_table`, `verify_user`, `ensure_wizard_accounts`, `disconnect_sql_server`, `g_ini_data`；另为 `Sql.h` 中仅有声明、`src/` 无实现的 `select_game_database`, `db_save_box`/`db_load_box`/`db_delete_box` 提供实现 |
| `mock_game.cpp` | 无（空壳占位） | — |
| `mock_itemmgr.cpp` | 物品管理器 | `load_item`, `load_fb`, `find_item`, `item_release`, `remove_item`, `random_vendor` |
| `mock_database.h` | `IDatabase` | `MockDatabase`, `ThrowingDatabase` |

由 `jianghu_core.lib` 提供真实实现、**不是** mock 的函数：`db_save_object`/`db_load_object`（`db/SqlContainer.cpp`）、`db_verify_password`（`db/SqlAccount.cpp`）、`CCreateDump`（`util/CreateDump.cpp`）、`find_online`（`net/Login.cpp`）。

**Mock 文件规范**：mock 头文件只含类声明、不含实现；实现放在对应的 `.cpp`；Mock 类提供 `reset()`/`reset_counts()`；记录调用次数与最后传入的参数，供测试断言。

### 可测试性设计：自由函数无法 Mock，必须类化

> **核心经验：编译进 `jianghu_core.lib` 的自由函数无法 Mock——需要外部依赖（DB/网络/文件）的自由函数，必须先重构为接口方法；纯计算的自由函数可直接测试，无需类化。**

**为什么自由函数无法 Mock：**

1. **链接器不支持替换**：静态库符号在链接时解析，测试代码调用 `load_item()` 时已绑定到库内实现，无法在链接时替换为 mock。
2. **同名符号覆盖不可行**：在 mock 中定义同名 `load_item()` 会报 `LNK2005: already defined`——该编译单元中其他被引用的符号（`find_item` 等）会把整个 .obj 拉入，造成重复定义。
3. **排除编译单元是错误方向**：把生产 .cpp 从库中排除、在测试工程里单独编译，改变了编译连接关系，测的不是同一份代码（见"禁止事项"）。

**解决方案：接口 + 全局指针依赖注入**

```
重构前:  CContainer* obj = load_item("sword");   // 自由函数，编译进 .lib，无法替换

重构后:  class IItemMgr { virtual CContainer* load_item(...) = 0; };  // 接口（生产头文件）
         class CItemMgr : public IItemMgr { ... };                     // 生产实现，编译进 .lib
         class CMockItemMgr : public IItemMgr { ... };                 // 测试 Mock，编译进 tests 工程
         extern IItemMgr* g_ItemMgr;                                   // 全局指针
         调用方: g_ItemMgr->load_item("sword");                // 测试注入 &mock
```

实施步骤：① 定义接口（纯虚基类）② 生产实现 ③ 声明全局指针 ④ 修改调用方 `load_item(name)` → `g_ItemMgr->load_item(name)` ⑤ 删除原自由函数（头文件与实现）⑥ 测试在 fixture 中注入 `g_ItemMgr = &mockItemMgr;`、`TearDown` 恢复。

用全局指针而非构造函数注入：JH 是大型遗留代码库，类之间耦合紧密，全局指针是侵入性最小的方案。

| 函数类型 | 是否需要类化 | 原因 |
|---|---|---|
| 自由函数 + 外部依赖（DB/网络/文件） | ✅ 必须 | 无法替换外部依赖 |
| 自由函数 + 纯计算逻辑 | ❌ 不需要 | 可直接测试，无需 mock |
| 类的虚函数 | ❌ 不需要 | 已可以通过继承 mock |
| 类的非虚函数 | ⚠️ 视情况 | 如果依赖外部，需要提取接口 |

**新增外部依赖的 Mock 流程**：① 生产头文件定义接口（纯虚 + `extern` 全局指针）② 生产实现（注册进 `jianghu_core.vcxproj`）③ `tests/mock/mock_<模块>.h` / `.cpp` 写 Mock 实现 ④ fixture 的 `SetUp()` 注入、`TearDown()` 恢复 ⑤ mock .cpp 注册进 `tests.vcxproj`。

**设计与 seam 的判据**：生产调用方也需要的接口是设计；只有测试需要的接口、全局指针、getter、reset 导出、注入参数是测试专用 seam——不要为了可测性向生产代码添加只有测试会用的东西。

**不要等到写测试时才发现无法 Mock——在设计阶段就考虑可测试性。**

### 依赖服务器代码的测试

测试文件可以直接 include 服务器源码头文件（如 `Container.h`、`Char.h`），tests 工程已配置了 server 源码的 include 路径。

所有服务器核心类（CContainer, CChar, CUser, CNpc, CItem, CSkill 等）的真实实现都在 `jianghu_core.lib` 中，测试可以直接使用。

### 注意事项

- 测试文件是独立编译单元，**不能** 直接编译服务器源码——实现来自链接的 `jianghu_core.lib`
- `EXPECT_EQ` 适用于整数、指针、枚举比较；浮点数请用 `EXPECT_NEAR(a, b, eps)` 或 `EXPECT_TRUE(fabs(a - b) < eps)`
- 需要结合 Lua 运行的测试，Lua 引擎已链接真实 `lua55.lib`，可在 Fixture 中调用 `CLuaEngine::init(scriptRoot, defaultScriptRoot)` 初始化。脚本相关的测试仅针对 `lua-default` 和 `lua-sample` 进行。

### 禁止事项

- ❌ 在生产代码中添加 `#ifdef _TESTING` 分支
- ❌ 在 `.vcxproj` 中排除生产 .cpp 来实现 mock
- ❌ 使用链接器 `/FORCE` 选项
- ❌ 在测试代码中 `#include` 生产 .cpp 文件
- ❌ 测试代码中使用 `sleep` 或硬编码延时
- ❌ 为测试向生产代码添加只有测试会用的导出、getter、reset（测试专用 seam）
- ❌ 在代码中保留注释形式的迁移记录

### 构建命令和运行方法

> ⚠️ **验证测试的唯一方式是运行 `.\run-coverage.ps1`（仓库根目录）。**
> **禁止**手动 `msbuild tests/tests.vcxproj` 后再直接运行 `tests.exe`。
> 原因：测试用相对路径加载配置（如 `ini_load("tests/test_config.ini")`），
> **必须从仓库根目录执行**才找得到 `test_config.ini` 与 Lua 脚本；手动进入
> `tests\Debug\` 运行 exe 会因工作目录错误导致配置/相对路径全部失效、测试大面积失败。
> `run-coverage.ps1` 已包办：定位 MSBuild、**以 `Release|Win32`（生产同构配置）编译**
> `jianghu_core.lib` + `tests`、**从仓库根目录执行测试**、采集覆盖率报告与 delta。
>
> **为什么绝对禁止 Debug：** Debug 与 Release 不是同一个被测对象。Debug 的栈填充区与调试堆会
> 暂时掩盖越界写/未初始化内存，优化开关又会改变 UB 的表现；生产代码跑的是 Release。
> Debug 全绿只证明 Debug 没问题，不证明生产没问题。因此 Debug 版测试程序被硬门禁封死：
> `TestConfigGate.DebugBuildIsNotAcceptance` 无条件失败，Debug 版无论怎么跑都在结果里报红。
>
> 聚焦调试：`run-coverage.ps1` 编译完成后从仓库根直接跑 `Release\tests.exe
> --gtest_filter=Suite.*`；
> **最终验收仍必须完整跑一遍 `.\run-coverage.ps1`**。

- **运行测试并收集覆盖率**：在仓库根目录执行 `.\run-coverage.ps1`（分支覆盖率：`.\run-coverage.ps1 -CoverageType branch`）。
- 仅需要编译服务器 `jianghu.exe`（不跑测试）时，才可手动构建：
  `& $msb jianghu.vcxproj /p:Configuration=Debug /p:Platform=Win32`。

### 覆盖度要求

**每次提交的测试覆盖率必须不低于上次提交。** 具体规则：

1. **不允许覆盖率下降**：运行覆盖率脚本后，`coverage_history/summary.csv` 中的 `delta` 列必须 ≥ 0。`delta` 只与**同构建配置**的上一条比较（`method` 列记录采集配置）；切换配置后的首条记录显示 `n/a` 并成为新基线。
2. **新增代码必须覆盖**：新增的函数/方法必须有对应的测试用例。
3. **修改代码必须验证**：修改的函数必须确保其修改的分支被测试覆盖。
4. **覆盖率报告作为提交依据**：提交代码时应附上覆盖率报告截图或数据，供 reviewer 确认。

覆盖率报告输出位置：
- **HTML报告**：`coverage_report/index.html`
- **历史数据**：`coverage_history/summary.csv`

阈值：
75%是及格线，90%是良好，95%以上是优秀。低于75%必须有合理解释（如测试难度大、外部依赖无法 mock 等）。

### 常见故障与排查

| 症状 | 原因 | 解决 |
|---|---|---|
| `error: SEH exception with code 0xc0000005` | 调用了未 mock 的真实函数（如 `load_item` 走到 `Create_Item`）；全局指针未初始化；fixture 未注入 mock | 检查 `g_ItemMgr` 等全局指针与 `SetUp()` 注入；用 `--gtest_filter` 缩小范围复跑 |
| `LNK2001: unresolved external symbol` | Mock 层缺少函数实现 | 在 `mock/*.cpp` 中补 stub |
| `LNK2005: already defined` | 符号同时存在于 lib 和 mock | 改为接口 + 依赖注入（见"可测试性设计"） |
| `LNK1181: cannot open input file` | 库文件路径错误 | 检查 `AdditionalLibraryDirectories` |
| 运行时崩溃、堆损坏 | `jianghu_core.lib` 与 `tests.exe` 使用了不同 CRT 版本 | 两者使用相同 CRT；以 `run-coverage.ps1` 的统一构建为准，不要手工混搭配置 |
| 单独运行通过、批量运行失败 | 测试间状态泄漏 | 在 `SetUp()` 中重置所有全局状态与 mock 计数器 |

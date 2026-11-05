# 单元测试架构设计

> 本文档是单元测试的**设计文档**：静态库 + Mock 架构与目录结构（§1）、可测试性设计（§2）、
> Debug 口径（§3）——即"为什么这么设计"，外加目录布局这份导航清单。
> 动手操作请看指令文件 `.claude/rules/unit-test.instructions.md`（规范、流程、命令、阈值、排查）。

---

## 1. 架构总览（静态库 + Mock 层）

### 1.1 组成

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

### 1.2 Mock 替代关系

| Mock 文件 | 替代的服务器文件 | 主要 mock 函数 |
|---|---|---|
| `mock_net.cpp` | 无（`net/Message.cpp` 直接编译进库，不 mock） | `g_test_suppress_output`（抑制测试输出） |
| `mock_db.cpp` | `db/Sql.cpp` + `main.cpp`（`g_ini_data` 定义处） | `get_db_type`, `init_sql_server`, `restore_data_table`, `verify_user`, `ensure_wizard_accounts`, `disconnect_sql_server`, `g_ini_data`；另为 `Sql.h` 中仅有声明、`src/` 无实现的 `select_game_database`, `db_save_box`/`db_load_box`/`db_delete_box` 提供实现 |
| `mock_game.cpp` | 无（空壳占位） | — |
| `mock_itemmgr.cpp` | 物品管理器 | `load_item`, `load_fb`, `find_item`, `item_release`, `remove_item`, `random_vendor` |
| `mock_database.h` | `IDatabase` | `MockDatabase`, `ThrowingDatabase` |

由 `jianghu_core.lib` 提供真实实现、**不是** mock 的函数：`db_save_object`/`db_load_object`（`db/SqlContainer.cpp`）、`db_verify_password`（`db/SqlAccount.cpp`）、`CCreateDump`（`util/CreateDump.cpp`）、`find_online`（`net/Login.cpp`）。

### 1.3 目录结构

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

---

## 2. 可测试性设计：自由函数为什么无法 Mock

**核心经验**：编译进 `jianghu_core.lib` 的自由函数无法 Mock——需要外部依赖（DB/网络/文件）的自由函数，必须先重构为接口方法；纯计算的自由函数可直接测试，无需类化。

**为什么自由函数无法 Mock：**

1. **链接器不支持替换**：静态库符号在链接时解析，测试代码调用 `load_item()` 时已绑定到库内实现，无法在链接时替换为 mock。
2. **同名符号覆盖不可行**：在 mock 中定义同名 `load_item()` 会报 `LNK2005: already defined`——该编译单元中其他被引用的符号（`find_item` 等）会把整个 .obj 拉入，造成重复定义。
3. **排除编译单元是错误方向**：把生产 .cpp 从库中排除、在测试工程里单独编译，改变了编译连接关系，测的不是同一份代码（指令文件「禁止事项」因此禁止该做法）。

**解决方案：接口 + 全局指针依赖注入**

```
重构前:  CContainer* obj = load_item("sword");   // 自由函数，编译进 .lib，无法替换

重构后:  class IItemMgr { virtual CContainer* load_item(...) = 0; };  // 接口（生产头文件）
         class CItemMgr : public IItemMgr { ... };                     // 生产实现，编译进 .lib
         class CMockItemMgr : public IItemMgr { ... };                 // 测试 Mock，编译进 tests 工程
         extern IItemMgr* g_ItemMgr;                                   // 全局指针
         调用方: g_ItemMgr->load_item("sword");                // 测试注入 &mock
```

用全局指针而非构造函数注入：JH 是大型遗留代码库，类之间耦合紧密，全局指针是侵入性最小的方案。

实施步骤、新增外部依赖的 Mock 流程与 seam 判据见指令文件「可测试性设计」节。

---

## 3. Debug 口径：Debug 与 Release 不是同一个被测对象

Debug 与 Release 不是同一个被测对象。Debug 的栈填充区与调试堆会暂时掩盖越界写/未初始化内存，优化开关又会改变 UB 的表现；生产代码跑的是 Release。Debug 全绿只证明 Debug 没问题，不证明生产没问题。

因此 Debug 版测试程序被硬门禁封死：`TestConfigGate.DebugBuildIsNotAcceptance` 无条件失败，Debug 版无论怎么跑都在结果里报红——门禁本身是指令文件「构建命令和运行方法」的可执行事实，本节是它的理由。

---

## 4. 执行模型：套件分片并行与按构造隔离

**目标**：单测规模增长后串行墙钟成为主要成本。`run-coverage.ps1` 把引擎测试按套件装箱成 N 个分片、与 CLI 测试路并行执行；同一批用例只是进程边界不同，因此结果口径不变，覆盖率口径以双标尺验收——**单片轮对串行基线逐行一致**（管线本身不引入覆盖差异）、**报告口径 delta 为 0**（分片轮对串行基线）。

**分片数是覆盖的一个输入维度**：随机生成的属性（如兽龄）决定的分支走向取决于进程内 `rand()` 流状态，进程边界改变流状态，少数随机结果相关行会随分片数互换——同拓扑多轮逐行完全确定，跨拓扑存在原始行数 ±数行的摆动，低于报告 0.1pp 粒度、不进 delta。结论：改变 `-Shards` 看到原始覆盖 ±数行属预期而非回归；等价性以单片逐行一致 + 报告口径 delta 双标尺判定。

**分片**：`--gtest_list_tests` 发现套件与用例数 → LPT 贪心装箱（用例数降序、并列按名序，结果与机器无关、可复现）→ 每片一个 `--gtest_filter`。装箱用例数之和与发现总数硬校验，空分片（filter 退化成「跑全部」）在启动前拦死。分片数由脚本 `param` 块的 `-Shards` 决定（0 = 自动取逻辑核数并封顶）。

**为什么按构造隔离，而不是靠「哪些套件有冲突」的名单**：测试对仓库的一切读写都相对 CWD 解析——配置加载、脚本根夹具的改写/改名（`ScopedSampleScript`、条件与数据脚本测试）、`map.txt`、日志目录。这些共享可变面是跨套件的（同一文件被不同套件读写），按套件切分不会让冲突边消失，两个套件落到不同分片就互踩。因此每个分片拿到一棵**私有的完整工作目录树**（拷贝配置、脚本根、教材目录 + 独立 `JIANGHU_TEST_TMP`），跨进程互踩在结构上不存在，也无需维护冲突名单。代价：拷贝清单成为「测试可触碰的仓库面」的登记——测试新增仓库相对路径的读写时，须同步进脚本的分片拷贝清单。

**DLL 依赖**：`tests.exe` 静态导入 MySQL 客户端库，而 Windows 的 DLL 搜索序把 CWD 排在 PATH 之前——换工作目录后私有树里若没有该 DLL，进程直接 `0xC0000135` 起不来。这是分片工作目录必须拷入 DLL 的机制依据。

**结果契约**：各分片的 gtest XML 是事实源，逐片三重校验——退出码、XML 失败数、执行数 == 发现数（filter 拼错、漏套件、进程崩溃都在此暴露）。任一片不过即整轮失败、不进合并，并保留分片工作目录与逐片 stdout/stderr 日志供排查。

**覆盖率合并契约**：各分片只导二进制 `.cov`，最终报告由一趟合并通行证产出（多路 `--input_coverage` 并集，child 用立即退出的空进程、不贡献覆盖）——与集测脚本同款。合并通行证的 child 退出码不可靠（OCC 0.9.x 会重建出 cmd 报错的命令行，集测同样如此），因此**以产物为闸门**：旧 `coverage.xml` 先删、合并后必须重新出现且行数非零，防止空报告污染 delta 历史。

**CLI 路**：CLI 测试全部经 `JIANGHU_TEST_TMP` 写临时目录、无仓库相对路径依赖，故作为独立一路与引擎分片同时跑、不参与装箱，工作目录保持仓库根。

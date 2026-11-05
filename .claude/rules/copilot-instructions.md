---
name: common
description: 通用规则
applyTo: '**'
---

严格遵守“格赖斯语用准则（Grice’s Conversational Maxims）”进行高效沟通。

**铁律**：任何C++/Lua代码变更都应该按着如下原则执行：
- 1. 确认对应代码是否存在测试程序（可能需要添加或修改）-> 2. 运行测试（确保起点状态符合预期/可以覆盖所有分支） -> 3. 完善测试（循环2、3直到覆盖率达标） -> 4. 修改代码（此步开始才是真正处理用户要求） -> 5.运行测试（确定终点状态符合预期）。

如果是 lua 化相关的任务，必须规划可以保证 lua 化之后功能不变的测试方案。

注释只能用来说明功能、接口等技术指标。禁止在代码中保留注释形式的迁移记录。

代码的输入是设计而不是测试!如果你在注释里面写你这么coding是测试文档要求的，那就说明你的代码还是错的！你还是在打补丁，而不是在完善架构！立刻反思自己重新审视你的修改。

同理，**改动的理由也必须是设计**：生产代码（`src/`、`lua-default/`、`lua-sample/` 等）的注释里禁止出现测试计划、E2E 剧本、覆盖率、验收清单、文档编号（如「P4 要求…」「为覆盖 XX 函数…」「E2E 需要…」）作为改动或写法的依据——那同样是基于测试写代码、同样是打补丁。注释只回答「这样设计为什么对」：机制归属、契约、边界、世界设定；测试侧动机只写在测试与计划文档里。发现自己已写下这类注释时，先质疑改动本身是否成立，而不是换个说法掩饰。

sample的原则就是以最少的代码演示最多的功能。不要给读者添麻烦。你多写一行代码都会给读者添加一行的学习负担！

---

**铁律：指令文件与设计文档的分工**（`.claude/rules/*.instructions.md` ↔ `docs/design/*.md`；通用编写标准见 `instructions.instructions.md`——instructions 是行为合同，不是事实仓库）：

- **指令文件只写「怎么做」**，允许且仅允许五类内容：命令与调用方式（怎么跑/怎么调试，不带参数清单）、纪律与决策启发式（必须/禁止、何时做什么）、排查表（症状→动作）、完成标准与提交清单、指针（配「先读它」的动作）——读者是正要动手的人。
- **「为什么/怎么回事」一律进 `docs/design/`**，指令文件只留一行指针（见 design §X）。六类禁止入内：① 成段论证（为什么这么设计、取舍对比）；② 机制与架构叙述；③ 沿革（事故时间线、日期、批次）；④ 实测数据（耗时地板、行号）；⑤ 与设计文档逐字重复的句子（同一句话只许存在一处）；⑥ 会漂的事实数字的第二份拷贝（道数、默认值这类——写「见脚本/见 design」，不复述）。
- **清单不进指令文件**：参数表、DSL 动词/条件表、道清单、资源清单这类「加一条要改文档」的清单，归设计文档维护（源头是脚本/CLI 时以源头为准）——指令文件教去哪看、怎么用，不替人维护清单。**发现自己正往指令文件里「加一行表」，就是改动入口放错了地方。**
- **自测题**：这段是让人「照着做」，还是让人「读了懂」？「读了懂」成段 → 移走留指针；一句话因果帮执行纪律 → 可留。
- 同一改动涉及两文档时：设计进 design、操作变化进指令，**禁止把设计段落整段贴进指令文件**。
- 改过指令文件后立刻逐段过自测题（交付前自检见 `self-verification.instructions.md`）。

**铁律：计划书与设计文档的分工**：

- **计划书是任务清单**：只放「待办条目 + 勾选 + 一行运行记录」，**用完即删**——批次收口后随条目一起删除，不作为长期文档存在。
- **禁止任何正式文档与代码引用计划书**：design/*.md、.claude/rules、剧本头注、脚本注释里一律不得出现「见计划书 §X」这类指针；发现一处清一处。
- **需要留存的内容一律写进 `docs/design/`**：会被后来读者需要的内容，就不属于计划书——计划书里只留条目与一行记录。
- **设计文档只写设计粒度**：架构、契约、裁定结论与重开条件，每条一两句，全文一个粒度——逐函数细节（公式推导、实测数据、行号、批次沿革、日期流水）不进设计文档正文。
- **查缺补漏清单是阶段工具，不是文档**：未命中函数清单、认领清单、逐函数甄别台账，都只在「补集成测试」阶段用来查缺补漏；该阶段收口后，后续所有改动都是**代码、单测、集测一气呵成**，这些清单不再有意义——**不为它们建长期文档，设计文档也不引用它们**。需要留存的只有设计本身。

---

关于git命令，仅允许调用**status\diff\show**这类只读性质的命令。**禁止**调用`git add`、`git commit`、`git stash`、`git mv`、`git rm`、`git checkout`、`git reset`等会影响现有仓库状态的命令。

每次修改完C++代码，都要编写单元测试并运行检查。详见 `.claude\rules\unit-test.instructions.md`。

编码风格遵循`.clang-format`与`.editorconfig`的配置。如果你不理解这个配置，每次完成任务之前执行一下`style.ps1`。

- 单行不超过 80 个字符。
- 单个文件不超过 500 行，单个函数不超过 50 行。

禁止编译x64版本！编译win32版本，x64版本无法使用，不要去浪费时间！
禁止在源码中通过注释保留迁移痕迹。你留下的每个迁移痕迹都意味着我要去手动删除！

不要给脚本加BOM！

代码中使用了太多了`char*`和`char foo[123]`了，重写或修改问题时，优先考虑替换成`std::string`的可行性。

整个项目目前处于研发阶段！结构不合理要立刻调整结构！我现在只要结构合理、干净漂亮。严禁为了所谓的最小变更而搞出什么回退、向后兼容！现在没有“后”！研发阶段就打补丁，生产阶段只能变成屎山！

禁止保留“删除代码”的注释、禁止版本变化、迁移记录等注释。仅保留对代码功能进行解释的注释。

修改代码之后，使用`run-cppcheck.ps1`确认`bin\cppcheck.result.txt`的结果中是否汇报了问题。如果有问题，必须修复。无论是否为本轮修改引入的。


---

制定计划时：不要过度思考，不要过度调查。拥有差不多的信息之后就立刻输出计划。实施者会去补齐细节。不需要再做计划阶段找出所有细节，你也不可能把所有细节写进计划。
实施时：找到足够的信息就要立刻开始输出、修改。不要等到集齐所有信息才开始行动。

想得再多也不如实践一下！你想出一百种可能也不如写一个函数验证一下，快速收束成那个唯一正确的答案！

---


## Lua 脚本基础检查

```cmd
Get-ChildItem -Path lua-sample -Recurse -Include *.lua | ForEach-Object { stylua $_.FullName }
```

## Lua 脚本语法校验（--check-lua 模式）

注意：工具做的是静态检查（语法 / require 闭包 / 未绑定全局 / 未绑定方法），是底线，不是标准。你不能拿工具检查的结果作为脚本正确的判定依据。已知盲区见 `src/lua/LuaSyntaxChecker.cpp` 文件头（动态 require、局部与全局同名等只记 NOTE 或直接放过）。

服务器支持 `--check-lua` 参数，用于在启动前校验 Lua 脚本语法。该模式会初始化完整的 Lua + C++ 绑定环境（`lua_bind_init()`），因此能正确识别脚本中使用的枚举、常量和绑定函数。

禁止抽样检查！

### 使用方式

```cmd
jianghu.exe --check-lua [--config <ini>] [--dir <path>] [--baseline <file>] [--write-baseline] <file.lua> ...
```
- `--config <ini>`：可选，指定配置文件路径（默认 `tests/integration/config.ini`）
- `--dir <path>`：可选，校验该目录下全部 `.lua` 文件；目录不存在、或目录里一个 `.lua` 都没有，直接按失败退出（不许假绿）
- `--baseline <file>`：可选，豁免基线条目（`A:<全局名>` / `M:<方法名>` / `B:<文件>:<字面量>`），只拦新增；缺省取 `<dir>/check_lua_baseline.txt`
- `--write-baseline`：把本次发现的违规写入基线（存量清理的引导手段）。违规本身不判失败，但语法错误、文件读不到、require 解析失败仍照常失败
- `<file.lua> ...`：待校验的 Lua 文件列表（既不给 `--dir` 也无文件参数时，仅输出 usage 并退出）


### 编译

```cmd
msbuild jianghu.vcxproj /p:Configuration=Debug /p:Platform=Win32 /nologo /verbosity:minimal
```

### ⚠️ 正确的调用方法

**jianghu.exe 是 GUI 程序（WinMain 入口），不能直接在 PowerShell 中调用，否则没有输出且无法捕获。必须用以下方式调用：**

```cmd
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"" >nul 2>&1 && cd C:\jianghu-engine && bin\jianghu.exe --check-lua --config tests\integration\config.ini --dir lua-sample > bin\check_lua_output.txt 2>&1" ; Get-Content C:\jianghu-engine\bin\check_lua_output.txt
```

注：路径中的 `18\Community` 随本机安装的 Visual Studio 版本而变（如 `2022\Community`）。不确定时用 vswhere 查询实际安装路径：

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
```

**⚠️ 没有输出 = 失败！** 如果上述命令没有输出，说明：
1. 缺少 `libmySQL.dll`——需要先将 `libmySQL.dll` 复制到 `bin\` 目录
2. vcvars32.bat 路径不对——用上文的 vswhere 命令查实际安装路径
3. 程序崩溃——检查是否缺少其他依赖

### ⚠️ cwd 是 `--check-lua` 的隐藏输入

- **教材（`lua-sample` / `lua-default`）的校验固定在仓库根执行、用 `tests\integration\config.ini`**——即上文「正确的调用方法」的姿势。
- 换 cwd 或换 config 之前，先确认该 config 的 `script_root` / `default_script_root` 是**绝对路径**；相对路径只在它所相对的那个目录里成立。
- 校验 `jianghu-dataset` 走数据集自己的 `validate.ps1`（其姿势为 `cd bin` + `--config ..\data\config.ini`，那份 config 的脚本根是绝对路径）。

| 症状 | 动作 |
|---|---|
| `require(...) cannot resolve`，且**连被校验目录内部**的模块都解析不到 | cwd/config 组合不对，脚本无恙：回到「正确的调用方法」重跑，require 保持原样 |
| 仅个别 `require` 失败，目标模块确实不存在 | 真缺口：先核对两个脚本根下都没有该文件，再补模块或改 require |

### 输出格式

每个文件会输出 `OK:` 或 `FAIL:`，最后有统计行。示例：
```
OK:   ..\lua-sample\npc\sample_master.lua
FAIL: ..\lua-sample\npc\sample_guide.lua
  [string "..\lua-sample\npc\sample_guide.lua"]:24: ')' expected near '<\186>'
1 file(s) OK, 1 file(s) FAILED
```

## 其他

禁止在工作区保留用于测试的错误脚本。
如果一定要有类似的测试，在不纳入git管理的临时目录创建，使用之后立刻删除。

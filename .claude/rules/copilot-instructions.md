---
name: common
description: 全工程常驻硬门禁与纪律：代码变更测试流程、注释与命名、构建/风格/cppcheck、git 只读、Lua check-lua 校验调用、脚本与临时文件编码
applyTo: '**'
---

严格遵守“格赖斯语用准则（Grice’s Conversational Maxims）”进行高效沟通。

**铁律**：任何C++/Lua代码变更都应该按着如下原则执行：
- 1. 确认对应代码是否存在测试程序（可能需要添加或修改）-> 2. 运行测试（确保起点状态符合预期/可以覆盖所有分支） -> 3. 完善测试（循环2、3直到覆盖率达标） -> 4. 修改代码（此步开始才是真正处理用户要求） -> 5.运行测试（确定终点状态符合预期）。

如果是 lua 化相关的任务，必须规划可以保证 lua 化之后功能不变的测试方案。

注释只能用来说明功能、接口等技术指标：禁止注释形式的迁移记录、迁移痕迹、被删代码与版本变化——那些都要人工删除。

代码的输入是设计而不是测试!如果你在注释里面写你这么coding是测试文档要求的，那就说明你的代码还是错的！你还是在打补丁，而不是在完善架构！立刻反思自己重新审视你的修改。

同理，**改动的理由也必须是设计**：生产代码（`src/`、`lua-default/`、`lua-sample/` 等）的注释里禁止出现测试计划、E2E 剧本、覆盖率、验收清单、文档编号（如「P4 要求…」「为覆盖 XX 函数…」「E2E 需要…」）作为改动或写法的依据——那同样是基于测试写代码、同样是打补丁。注释只回答「这样设计为什么对」：机制归属、契约、边界、世界设定；测试侧动机只写在测试与计划文档里。发现自己已写下这类注释时，先质疑改动本身是否成立，而不是换个说法掩饰。

**铁律：** 1. sample的原则就是以最少的代码演示最多的功能。不要给读者添麻烦。你多写一行代码都会给读者添加一行的学习负担！ 2. sample 中的每一个功能、每一个调用都要符合事实逻辑。不能为了演示某个函数而生硬的拼接一些不符合一般逻辑的调用。

---

- 每隔几次工具调用，用一行话告诉用户现在进展到哪了。
- 直接回答，不要斟酌。

---

**铁律：指令文件与设计文档的分工**：完整条款（允许五类、禁止六类、清单不进指令文件、交付前自检与自测题）见 `instructions.instructions.md`。

**铁律：技能层与设计文档的分工**：技能是按需触发的操作规程（触发才进上下文），承载完整工作流、纪律、排查表与提交清单；「为什么」进 `docs/design/` 留指针，会漂清单不进技能。**无条件生效的硬门禁与导航留在常驻层**（`AGENTS.md`、规则文件），不因技能未触发而失守。

**铁律：计划书与设计文档的分工**：

- **计划书是任务清单**：只放「待办条目 + 勾选 + 一行运行记录」，**用完即删**——批次收口后随条目一起删除，不作为长期文档存在。
- **禁止任何正式文档与代码引用计划书**：design/*.md、.claude/rules、剧本头注、脚本注释里一律不得出现「见计划书 §X」这类指针；发现一处清一处。
- **需要留存的内容一律写进 `docs/design/`**：会被后来读者需要的内容，就不属于计划书——计划书里只留条目与一行记录。
- **设计文档只写设计粒度**：架构、契约、裁定结论与重开条件，每条一两句，全文一个粒度——逐函数细节（公式推导、实测数据、行号、批次沿革、日期流水）不进设计文档正文。
- **查缺补漏清单是阶段工具，不是文档**：未命中函数清单、认领清单、逐函数甄别台账，都只在「补集成测试」阶段用来查缺补漏；该阶段收口后，后续所有改动都是**代码、单测、集测一气呵成**，这些清单不再有意义——**不为它们建长期文档，设计文档也不引用它们**。需要留存的只有设计本身。

---

关于git命令，仅允许调用**status\diff\show**这类只读性质的命令。**禁止**调用`git add`、`git commit`、`git stash`、`git mv`、`git rm`、`git checkout`、`git reset`等会影响现有仓库状态的命令。

每次修改完C++代码，都要编写单元测试并运行检查。详见 `.claude/skills/unit-test/SKILL.md`。

编码风格遵循`.clang-format`与`.editorconfig`的配置。如果你不理解这个配置，每次完成任务之前执行一下`style.ps1`。

- 单个文件不超过 500 行，单个函数不超过 50 行。

编译与测试默认走 x64 版本（生产同构平台）；Win32 版本仍可编译，但只在显式指定时构建。

不要给lua脚本加BOM！

代码中使用了太多了`char*`和`char foo[123]`了，重写或修改问题时，优先考虑替换成`std::string`的可行性。

整个项目目前处于研发阶段！结构不合理要立刻调整结构！我现在只要结构合理、干净漂亮。严禁为了所谓的最小变更而搞出什么回退、向后兼容！现在没有“后”！研发阶段就打补丁，生产阶段只能变成屎山！

修改代码之后，使用`run-cppcheck.ps1`确认`bin\cppcheck.result.txt`的结果中是否汇报了问题。如果有问题，必须修复。无论是否为本轮修改引入的。

改动 `src\lua\LuaBind.cpp` 绑定表（增删条目/改名）后，运行仓库根 `.\check-binds.ps1`：四方对账（表项 ↔ decl ↔ impl ↔ 测试引用）必须 OK，重复登记与 PascalCase 告警必须为 0，严格口径调用点报表落在 `test_tmp\bind_usage.txt`。

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

服务器支持 `--check-lua` 参数，用于在启动前校验 Lua 脚本语法；校验在完整绑定环境中进行，脚本用到的枚举、常量、绑定函数都在识别范围内。

禁止抽样检查！

### 使用方式

```cmd
jianghu.exe --check-lua [--config <ini>] [--dir <path>] [--baseline <file>] [--write-baseline] <file.lua> ...
```

参数语义不在此维护：以 `src/lua/LuaSyntaxChecker.h` 的参数注释与 `LuaSyntaxChecker.cpp` 的基线键注释为准（无 `--dir` 且无文件参数时程序自打印 usage 摘要）。

### 编译

校验用的可执行文件是 **`bin\e2e\jianghu.exe`**（`bin\jianghu.exe` 是功能测试留下的过期版本，不作校验依据）。改过 `src` 后重建它，或直接跑 `.\run-integration-tests.ps1`（每轮都会重建）：

```cmd
msbuild jianghu.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=C:\jianghu-engine\bin\e2e\ /nologo /verbosity:minimal
```

### ⚠️ 正确的调用方法

**jianghu.exe 是 GUI 程序（WinMain 入口），不能直接在 PowerShell 中调用，否则没有输出且无法捕获。必须先用 vswhere 定位本机 Visual Studio，再进其构建环境调用：**

```powershell
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
cmd /c "call ""$vs\VC\Auxiliary\Build\vcvars64.bat"" >nul 2>&1 && cd C:\jianghu-engine && bin\e2e\jianghu.exe --check-lua --config tests\integration\config.ini --dir lua-sample > bin\check_lua_output.txt 2>&1"
Get-Content C:\jianghu-engine\bin\check_lua_output.txt
```

**⚠️ 没有输出 = 失败！** 如果上述命令没有输出，说明：
1. 缺少 `libmySQL.dll`——需要先将 `libmySQL.dll` 复制到 exe 同目录（`bin\e2e\`）
2. vswhere 找不到 Visual Studio 安装——确认本机装有带 C++ 工具链的 VS
3. 程序崩溃——检查是否缺少其他依赖

### ⚠️ cwd 是 `--check-lua` 的隐藏输入

- **教材（`lua-sample` / `lua-default`）的校验固定在仓库根执行、用 `tests\integration\config.ini`**——即上文「正确的调用方法」的姿势。
- 换 cwd 或换 config 之前，先确认该 config 的 `script_root` / `default_script_root` 是**绝对路径**；相对路径只在它所相对的那个目录里成立。
- 校验 `jianghu-dataset` 走数据集自己的 `validate.ps1`（其姿势为 `cd bin` + `.\e2e\jianghu.exe --config ..\config.ini`，那份 config 的脚本根是绝对路径）。

| 症状 | 动作 |
|---|---|
| `require(...) cannot resolve`，且**连被校验目录内部**的模块都解析不到 | cwd/config 组合不对，脚本无恙：回到「正确的调用方法」重跑，require 保持原样 |
| 仅个别 `require` 失败，目标模块确实不存在 | 真缺口：先核对两个脚本根下都没有该文件，再补模块或改 require |

## 脚本与临时文件编码

写 `.ps1`、临时文件、工具输出时按下面走，编码问题不进控制台、不进判读依据。

- **`write` 工具落盘的文件恒为 UTF-8 无 BOM**：往 `.ps1` 写中文即触发下表第一行，动笔前先决定本脚本走纯 ASCII 还是 BOM。
- **`.ps1` 优先写成纯 ASCII**：中文只出现在正则字符类或少量字符串时，用 `[char]0x4E00` 形式的转义拼出；中文成段、非 ASCII 绕不开时按 `AGENTS.md`「文件编码（BOM）」条款加 UTF-8 BOM。
- **控制台回显的中文一律按乱码对待**：需要判读的输出写进 UTF-8 无 BOM 的临时文件，用 read 工具读。
- **临时文件用 `[IO.File]::WriteAllText($p, $t, (New-Object Text.UTF8Encoding($false)))` 落盘**；PS 5.1 的 `Out-File -Encoding utf8` 写出的是带 BOM 文件。
- **同一错误重复刷屏时先截断**：`Select-Object -Last N` 取尾部，或整体落文件后再读，逐条读输出不产生判读价值。

| 症状 | 动作 |
|---|---|
| `.ps1` 报 `Unterminated [] set`，且报错行里的字符类是乱码 | 脚本含非 ASCII 且无 BOM：改纯 ASCII 转义，或按 `AGENTS.md` 加 UTF-8 BOM |
| 工具回显中文为乱码 / `????` | 结果写 UTF-8 无 BOM 临时文件，用 read 工具读；控制台内容不作判读依据 |
| 输出数万行、几乎全是同一条报错 | `Select-Object -Last N` 取尾部或整体落文件，先定位首条真实报错 |

## 其他

禁止在工作区保留用于测试的错误脚本。
如果一定要有类似的测试，在不纳入git管理的临时目录创建，使用之后立刻删除。

---
name: common
description: 通用规则
applyTo: '**/*.cpp,**/*.h'
---

**铁律**：任何C++/Lua代码变更都应该按着如下原则执行：
- 1. 确认对应代码是否存在测试程序（可能需要添加或修改）-> 2. 运行测试（确保起点状态符合预期/可以覆盖所有分支） -> 3. 完善测试（循环2、3直到覆盖率达标） -> 4. 修改代码（此步开始才是真正处理用户要求） -> 5.运行测试（确定终点状态符合预期）。

如果是 lua 化相关的任务，必须规划可以保证 lua 化之后功能不变的测试方案。

注释只能用来说明功能、接口等技术指标。禁止在代码中保留注释形式的迁移记录。

---

关于git命令，仅允许调用**status\diff\show**这类只读性质的命令。**禁止**调用`git add`、`git commit`、`git stash`、`git mv`、`git rm`、`git checkout`、`git reset`等会影响现有仓库状态的命令。

每次修改完C++代码，都要编写单元测试并运行检查。详见 `.agents\instructions\unit-test.instructions.md`。

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

注意：工具是只检查最基础的语法，是底线，不是标准。你不能拿工具检查的结果作为脚本正确的判定依据。

服务器支持 `--check-lua` 参数，用于在启动前校验 Lua 脚本语法。该模式会初始化完整的 Lua + C++ 绑定环境（`lua_bind_init()`），因此能正确识别脚本中使用的枚举、常量和绑定函数。

禁止抽样检查！

### 使用方式

```cmd
jianghu.exe --check-lua [--config <ini>] [--dir <path>] <file.lua> ...
```
- `--config <ini>`：可选，指定配置文件路径（默认 `data/config.ini`）
- `--dir <path>`：可选，校验该目录下全部 `.lua` 文件
- `<file.lua> ...`：待校验的 Lua 文件列表（既不给 `--dir` 也无文件参数时，仅输出 usage 并退出）

### 编译

```cmd
msbuild jianghu.vcxproj /p:Configuration=Debug /p:Platform=Win32 /nologo /verbosity:minimal
```

### ⚠️ 正确的调用方法

**jianghu.exe 是 GUI 程序（WinMain 入口），不能直接在 PowerShell 中调用，否则没有输出且无法捕获。必须用以下方式调用：**

```cmd
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat"" >nul 2>&1 && cd C:\jianghu-engine\bin && .\jianghu.exe --check-lua --config ..\data\config.ini file1.lua file2.lua > C:\jianghu-engine\bin\check_lua_output.txt 2>&1" ; Get-Content C:\jianghu-engine\bin\check_lua_output.txt
```

注：路径中的 `18\Community` 随本机安装的 Visual Studio 版本而变（如 `2022\Community`）。不确定时用 vswhere 查询实际安装路径：

```powershell
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
```

**⚠️ 没有输出 = 失败！** 如果上述命令没有输出，说明：
1. 缺少 `libmySQL.dll`——需要先将 `libmySQL.dll` 复制到 `bin\` 目录
2. vcvars32.bat 路径不对——用上文的 vswhere 命令查实际安装路径
3. 程序崩溃——检查是否缺少其他依赖

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

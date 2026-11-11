# Agent Guidelines

## 代码来源（铁律）

**代码的来源是设计，不是测试！基于测试写代码就是打补丁，只有基于设计的修改才是正确的。禁止打补丁！**

- 生产代码（`src/`、`lua-default/`、`lua-sample/` 等）的每处改动理由必须是**设计**，
  注释里不得出现测试计划、E2E 剧本、覆盖率、验收清单、文档编号这类测试侧依据。
  完整条款见 `.claude/rules/copilot-instructions.md` 的编码铁律节。

## 文档分工（铁律）

**指令文件（`.claude/rules/*.instructions.md`）只写怎么做；为什么、机制、沿革进 `docs/design/`，指令侧留指针。**
完整条款见 `.claude/rules/instructions.instructions.md`；
改过指令文件后按 `.claude/rules/self-verification.instructions.md` 逐段自检。

## 编码规范/风格

遵循`.claude/rules/copilot-instructions.md`中的指令文件。
遵循`.claude/rules/naming.instructions.md`中的指令文件。

### 文件编码（BOM）

- 新增与修改的文件一律 UTF-8 **无 BOM**；存量带 BOM 的文件**改到即改对**，不做全量清理。
- `.ps1` 是唯一例外：内容含非 ASCII 时**必须带 UTF-8 BOM**。PS 5.1 只靠 BOM 识别
  UTF-8，缺失时按 GBK 解码，中文会吃掉字符串引号，脚本直接解析失败（纯 ASCII 的
  `.ps1` 无 BOM 同样可运行）。
- 工程已全工程配置 `/source-charset:utf-8`，源码带不带 BOM 都不影响编译；BOM 属
  一致性约定，不属构建要求。

## 测试

单元测试遵循技能 `.claude/skills/unit-test/SKILL.md`；测试价值评审与修剪遵循技能
`.claude/skills/test-audit/SKILL.md`；集成测试遵循技能
`.claude/skills/integration-test/SKILL.md`。动手前先加载对应技能。

## 进程安全

遵循：`.claude/rules/process-safety.instructions.md`
（禁止按进程名杀进程；终止必须按 PID / 自有句柄，并核对监听端口）

## 其他资产

技能目录：`.claude/skills`
指令目录：`.claude/rules`

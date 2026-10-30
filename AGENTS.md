# Agent Guidelines

## 代码来源（铁律）

**代码的来源是设计，不是测试！基于测试写代码就是打补丁，只有基于设计的修改才是正确的。禁止打补丁！**

- 生产代码（`src/`、`lua-default/`、`lua-sample/` 等）的每处改动，理由必须是**设计**：
  机制归属、契约、边界、世界设定。注释里禁止出现测试计划、E2E 剧本、覆盖率、
  验收清单、文档编号（如「P4 要求…」「为覆盖 XX 函数…」「E2E 需要…」）作为改动
  或写法的依据——在注释里写“这么改是测试要的”，就说明改动还是错的：那不是完善
  架构，是打补丁，立刻反思并重新审视整个修改。
- 测试侧的动机只写在测试与计划文档里，不写进生产代码。
- 完整条款见 `.claude/rules/copilot-instructions.md` 的编码铁律节。

## 编码规范/风格

遵循`.claude/rules/copilot-instructions.md`中的指令文件。
遵循`.claude/rules/naming-convention.instructions.md`中的指令文件。

## 测试

遵循：`.claude/rules/unit-test.instructions.md`

## 进程安全

遵循：`.claude/rules/process-safety.instructions.md`
（禁止按进程名杀进程；终止必须按 PID / 自有句柄，并核对监听端口）

## 其他资产

技能目录：`.claude/skills`
指令目录：`.claude/rules`

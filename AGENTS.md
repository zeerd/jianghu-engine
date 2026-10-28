# Agent Guidelines

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

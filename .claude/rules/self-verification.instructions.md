---
name: 'Self Verification'
description: '交付前自检：代码落地后（迁移记录注释、style.ps1）与文档落地后（行为合同自检、指令与设计分工自测题、对句查重与交叉指针核对）'
applyTo: '**'
---

# 交付前自检

## 代码落地后

- 检查注释里是否写了迁移记录；写了就改成对功能/接口本身的说明。
- 检查本轮是否运行过 `style.ps1`；没跑就补跑。

## 文档落地后（动过 `.claude/rules/*.instructions.md` 时）

- 按 `instructions.instructions.md` §4「交付前自检」清单逐项过一遍（含「照着做 / 读了懂」自测题、与设计文档的对句查重、交叉指针核对）。

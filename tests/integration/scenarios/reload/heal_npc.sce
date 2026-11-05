# heal_npc.sce — 修复恢复（第一段，用 c 变体）：坏文件换回合法变体
#
# 错误态（hasError）只在成功重载时清除——它是下一次失败广播的边沿前提，
# 故每个失败片段之后都必须有一次成功恢复，否则后续失败不再广播、
# 片段就没有可等的屏障。恢复同时把内容推到 C 版（丙），行为链式前进。
#
# ⚠ 与 heal_npc_d.sce 交替使用：内容哈希是重载的最终判据，重复写入同一
# 内容会被 noop 分支吞掉（等不到成功广播）。

clear
filecopy $fixtures/c/npc/e2e_reload_npc.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载完成：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

# heal_npc_d.sce — 修复恢复（第二段，用 d 变体）
#
# 与 heal_npc.sce 行为完全一致（应答同为丙），但写入的字节必须不同：
# 自动重载先过 mtime 快筛、再比内容哈希——上一段 heal 已把内容写成 c，
# 这里再写 c 会被判成「mtime 变、内容未变」的 noop，等不到成功广播。
# 链内所有 heal/恢复片段按 c → d → c → d 交替，保证每次写入相对当前
# 内容必变。

clear
filecopy $fixtures/d/npc/e2e_reload_npc.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载完成：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

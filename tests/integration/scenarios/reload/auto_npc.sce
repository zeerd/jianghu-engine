# auto_npc.sce — 自动重载主路径：改文件后心跳切换行为（全程不敲 reload 命令）
#
# 等的是重载结果的 sys 广播（5s 心跳扫描的确定性屏障，不是 sleep 盲等）；
# 广播到达后再断言询问应答换了新标记——「重载发生了」与「行为真的换了」
# 两层都过才算过。
#
# clear 在 filecopy 之前：wait 扫累积缓冲，上一片段留下的同名广播会让
# 新 wait 在重载发生前就误通过。

clear
filecopy $fixtures/b/npc/e2e_reload_npc.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载完成：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC乙】 timeout=10000
cmd /t 0

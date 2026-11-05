# fail_missing.sce — 文件缺失兜底：读不到文件不崩、旧版本继续服役
#
# 进入本片段前 heal_npc.sce 已清错误态，删除文件的失败边沿广播可等。
# NPC 实例持有旧 ref，文件删除不影响它继续应答（丙）。

clear
filedelete $scripts/npc/e2e_reload_npc.lua
wait text 自动重载失败，保留旧版本：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

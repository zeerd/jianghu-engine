# fail_runtime.sce — 运行时错误兜底：chunk 编译过、执行抛错，同样保留旧版
#
# 进入本片段前 heal_npc.sce 已把错误态清掉，故这次失败的边沿广播会再发
# 一次（可等的屏障）。行为断言：询问仍得丙（上一段恢复后的版本）。

clear
filecopy $fixtures/broken/npc/runtime_error.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载失败，保留旧版本：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

# cmd_fail.sce — 手动 reload 的失败回执必须如实
#
# 旧实现拿 ref 真值判断：失败保留的旧 ref(≥0) 与 LUA_NOREF(-2) 都为真，
# 任何失败都回「已重载」，「失败或未加载」文案不可达。此处断言失败文案
# 真的可达，且失败不换版本（询问仍丙）。
#
# 本片段进入时错误态仍在（上一片段是删除文件的失败），心跳重试不发广播，
# 回执断言不会被广播串扰；clear 仍放在最前，挡住更早的遗留输出。

clear
filecopy $fixtures/broken/npc/syntax_error.lua $scripts/npc/e2e_reload_npc.lua
cmd /reload e2e_reload_npc
wait text 重载失败或未加载 timeout=10000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

# cmd_ok.sce — 手动 reload 成功回执 + 新行为
#
# 写回合法 C 版后手动 reload：回执「Lua 脚本已重载。」，询问得丙。
# 即便心跳抢先自动重载了同内容（广播文案不同、且回执仍会由手动 reload
# 重新执行文件给出），本片段的两个断言都成立，无竞态。

clear
filecopy $fixtures/c/npc/e2e_reload_npc.lua $scripts/npc/e2e_reload_npc.lua
cmd /reload e2e_reload_npc
wait text Lua 脚本已重载。 timeout=10000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

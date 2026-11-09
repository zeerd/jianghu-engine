# fail_syntax.sce — 语法错误兜底：旧版本保留 + 失败广播边沿不刷屏
#
# 写坏文件 → 心跳尝试重载 → 失败广播（边沿：错误态 false→true 才报）→
# 询问仍得乙（旧版本活着，实例没被换版本也没被无谓重建）。
#
# 随后 notext 监视 13 秒：失败尝试落观测态后，坏文件在再次变化前不重试；
# 即便重试，边沿（错误态 false→true 才报）也保证窗口内不出现第二条失败
# 广播——出现即红。notext 前必须 clear：首条失败广播还在累积缓冲里，
# 不清会当场误判。

clear
filecopy $fixtures/broken/npc/syntax_error.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载失败，保留旧版本：e2e_reload_npc timeout=20000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC乙】 timeout=10000
cmd /t 0

clear
wait notext 自动重载失败，保留旧版本：e2e_reload_npc timeout=13000

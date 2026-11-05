# auto_item.sce — 物品实例刷新：背包里的实例随脚本重载切换新逻辑
#
# 重载通知按 bareName 刷所有存活实例（含背包内），无需重新获取；
# clear 在 filecopy 之前，理由同 auto_npc.sce。

clear
filecopy $fixtures/b/item/e2e_reload_item.lua $scripts/item/e2e_reload_item.lua
wait text 自动重载完成：e2e_reload_item timeout=20000

cmd /t e2e_reload_item
clear
cmd /use
wait text 【物品乙】 timeout=10000
cmd /t 0

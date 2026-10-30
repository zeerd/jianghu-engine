# open_boxes.sce — 开箱取物：容器 look 弹菜单 → get from
#
# 覆盖：CRoom 房间容器（木箱/药箱）的 look 菜单与取物链路。
# 判定：每取一件都断言背包里真的多了这件东西（wait item），光选中菜单不算数。
#
# ⚠ 并行隔离：木箱（铁剑 ×1 + 金币 ×60000）与药箱（金创药 ×3）都是
#   create() 里一次性放置、且不随 CRoom::reset 恢复的共享资源——取走即止，
#   全服只有一份。因此开箱段只由 L1 一道执行；其余各道不碰箱子，
#   改从商人购买所需物品，各靠启动本金自给自足。

cmd /t mu xiang
cmd /look
wait menu
menu 铁剑
wait item 铁剑 timeout=10000
cmd /t mu xiang
cmd /look
wait menu
menu 金币
wait item 金币 timeout=10000
cmd /t yao xiang
cmd /look
wait menu
menu 金创药
wait item 金创药 timeout=10000
cmd /t 0

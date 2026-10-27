# tea_fill_drink.sce — 茶棚：fill 装水 + use 饮用
#
# 覆盖：room/sample_tea_stall.lua 的 resource/water 装水源，与
#   lua-default/item/liquid_base.lua 的 do_fill / do_use / do_look。
# 判定（每步都有回执，缺一步就红）：
#   房间观察 → 私发的场景描述「这里可以给容器装水」
#   fill      → 「你将茶壶装满清茶」（本人版；证明装填真的发生了）
#   use       → 「你拿起茶壶咕噜噜地喝了几口清茶」（本人版）
#   再观察    → 「里面装了七、八分满的清茶」——装满 10 口喝掉 1 口，
#               这句由 liquid/remaining 推导，能同时证明 fill 装满了、use 少了
# 说明：茶棚房间本身不放茶壶，茶壶须先由本道从商人购得（mountain_foot/buy_sell.sce）。

cmd /t 0
cmd /go 山脚茶棚
wait room 山脚茶棚
clear
cmd /look
wait text 这里可以给容器装水 timeout=10000

cmd /t 茶壶
clear
cmd /fill
wait text 你将茶壶装满清茶 timeout=10000
sleep 600

cmd /t 茶壶
clear
cmd /use
wait text 你拿起茶壶咕噜噜地喝了几口清茶 timeout=10000
sleep 600

cmd /t 茶壶
clear
cmd /look
wait text 里面装了七、八分满的清茶 timeout=10000
cmd /t 0

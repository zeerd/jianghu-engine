# tea_fill_drink.sce — 茶棚：fill 装茶（收费扣存款）+ use 饮用
#
# 覆盖：room/sample_tea_stall.lua 的 resource/water 装水源（water_type=tea、
#   resource/fee=10——茶棚营业装茶收茶钱，liquid_base.do_fill 的 fee>0 记账
#   分支从钱庄存款扣费），lua-default/item/liquid_base.lua 的
#   do_fill / do_use / do_look；顺带 CChar::add_balance（扣费路径）。
#   tea 类饮品不触发任何 condition（演示纯液体容器用法，无状态副作用）。
# 判定（每步都有回执/状态，缺一步就红）：
#   房间观察 → 私发的场景描述「这里可以给容器装水」
#   fill（存款 0）→ 「穷光蛋！」负支（notify_fail 仅本人可见）
#   钱庄存款 100 → 本人版「你存入现款100金币」
#   fill（存款 100）→ 本人版「你高喊道：老板，记在账上！」（fee>0 扣费成功）
#     +「你将酒壶装满清茶」（装填真的发生了）
#   /balance → 「你现在共有存款：90金币」——扣费是**状态**断言：100-fee=90
#     （fee=10 见 sample_tea_stall.lua；只断「记在账上」回执证明不了钱真扣了）
#   use       → 「你拿起酒壶咕噜噜地喝了几口清茶」（本人版）
#   再观察    → 「里面装了七、八分满的清茶」——装满 10 口喝掉 1 口，
#               这句由 liquid/remaining 推导，能同时证明 fill 装满了、use 少了
# 说明：茶棚房间本身不放酒壶，酒壶须先由本道从商人购得（buy_sell.sce）。
#   存款扣费故本道自办存取（bank.sce 是 L4 的片段，本道只存不取，数额独立）。
# 并行说明：「记在账上」等回执都是 message_vision 本人版——同房间别道账号
#   看到的是「<名字>高喊道」，不会误满足本道的「你高喊道」断言。

cmd /t 0
cmd /go 山脚茶棚
wait room 山脚茶棚
clear
cmd /look
wait text 这里可以给容器装水 timeout=10000

# 新号存款为 0 → fee>0 的拒绝支：装不了水（负支先行，与成功支成对）
cmd /t cha hu
clear
cmd /fill
wait text 穷光蛋 timeout=10000

# 去钱庄存 100（起步现金有余），为扣费成功支铺路
# 茶棚只有回山脚的一扇门，去钱庄须先回山脚再走（房间拓扑，/go 只走直达门）
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚
cmd /go 山脚钱庄
wait room 山脚钱庄
wait object 钱庄老板
cmd /t lao ban
clear
cmd /deposit 100
wait text 你存入现款100金币 timeout=10000

# 回茶棚装茶：fee>0 扣款分支 + 装填回执
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚
cmd /go 山脚茶棚
wait room 山脚茶棚
cmd /t cha hu
clear
cmd /fill
wait text 你高喊道：老板，记在账上 timeout=10000
wait text 你将酒壶装满清茶 timeout=10000

# 回钱庄核账：存款 100 - fee 10 = 90（状态断言，证明扣费真发生了）
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚
cmd /go 山脚钱庄
wait room 山脚钱庄
wait object 钱庄老板
cmd /t lao ban
clear
cmd /balance
wait text 你现在共有存款：90金币 timeout=10000

# 回茶棚饮用（item_ops.sce 接续在茶棚丢壶）
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚
cmd /go 山脚茶棚
wait room 山脚茶棚
cmd /t cha hu
clear
cmd /use
wait text 你拿起酒壶咕噜噜地喝了几口清茶 timeout=10000

cmd /t cha hu
clear
cmd /look
wait text 里面装了七、八分满的清茶 timeout=10000
cmd /t 0

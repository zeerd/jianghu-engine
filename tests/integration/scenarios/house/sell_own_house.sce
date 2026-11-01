# sell_own_house.sce — 属区菜单直卖自家宅院（置产链的最小收口）
#
# 业务：住户回到住宅区，观察菜单里直接卖掉名下房产 → 估价 → 决定卖了 →
#   注销住户登记（属区名额回收，登记册每轮净 0）。与 house/rename_and_sell.sce
#   的卖房块同一 `sellhouse 0 0` 入口，区别是**不入宅**——不碰房内任何状态，故
#   没有宅内前置（改名/拒客/欢迎词都不需要）；供「置业只为一次性用途、用完即弃」
#   的道使用：本道置业仅为 invite_a 的做客邀请，用完即卖，不留跨轮名额。
#
# 判定：估价回执 + 本人版「你卖掉了」；卖房后本道即收工，住户行与房实例由
#   remove_house 一并注销，无跨轮残留、无房内残留。
#
# 动线前提：本道此刻所在房与山脚有直达门（pk/会合收在山脚茶棚，茶棚只有回山脚
#   一扇门 → 两跳：茶棚 → 山脚 → 属区）。换房后菜单晚于 wait room 一拍，
#   故先 /look 等 menu 再按文本选（§8.5 的「先等到再定向」同理）。
# 目标纪律：全程不改 /t（观察菜单走 menu 文本），块首无需 /t 0。

cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
cmd /go 青云城住宅区
wait room 青云城住宅区 timeout=15000
clear
cmd /look
wait menu timeout=10000
menu 卖掉雅室
wait text 目前价值 timeout=10000
wait menu
menu 决定卖了
wait text 你卖掉了 timeout=10000

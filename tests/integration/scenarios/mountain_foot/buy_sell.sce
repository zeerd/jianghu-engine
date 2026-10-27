# buy_sell.sce — 商人：货物清单 / 购买 / 出售
#
# 覆盖：CVendor::do_list / do_buy / do_sell（含 pre_buy / post_buy 挂点）。
# 判定：买卖各断言本人专属回执——`message_vision` 对本人渲染「你…」、对房间
#   其他人渲染「<名字>…」，所以「你从杂货老板那里买下了…」只有自己买成功时
#   才会出现，山脚多账号并行采购时不会被别人的成交串起来。
#
# 本段只备齐 L1 自己要用的物品：
#   金创药（商品 5）买 3 包 —— 任务交物 1 包、房屋出租 1 包、卖给出售分支 1 包
#   茶壶  （商品 9）买 1 把 —— 茶棚段 fill/use
# 学艺补气的回春丹、断肠散、传送符、棋子等由各自那道自行购买。

cmd /t 杂货老板
cmd /list
sleep 600
repeat 3
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
sleep 600
end
clear
cmd /buy 9
wait text 你从杂货老板那里买下了数量1的茶壶 timeout=10000
sleep 600
clear
cmd /sell 金创药
wait text 你卖掉了数量1的金创药 timeout=10000
sleep 600
cmd /t 0

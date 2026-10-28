# social_supplies.sce — L4（社交道）在山脚商人的补给采购
#
#   断肠散（商品 7）买 1 包 —— 毒药/补药对照的中毒源（全服仅本道服用）
#   金创药（商品 5）买 5 包 —— 中毒后循环补满气血上限（单次最多补 100，未必一次到位）
#     + 验证「不需要用」拒绝分支
#   回春丹（商品 6）买 8 粒 —— 中毒后回气血循环（同样可能要几粒）+ 「不需要用」拒绝分支
#   传送符（商品 8）买 1 张 —— 传送菜单与目的地选择
#
# 判定：首件断言本人专属成交回执，收尾断言背包里确有断肠散与传送符。

cmd /t 杂货老板
cmd /list
sleep 600

clear
cmd /buy 7
wait text 你从杂货老板那里买下了数量1的断肠散 timeout=10000
sleep 600

repeat 5
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
sleep 600
end

repeat 8
clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
sleep 600
end

clear
cmd /buy 8
wait text 你从杂货老板那里买下了数量1的传送符 timeout=10000
sleep 600

wait item 断肠散 timeout=10000
wait item 传送符 timeout=10000
cmd /t 0

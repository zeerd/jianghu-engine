# combat_supplies.sce — L3（战斗道）在山脚商人的补给采购
#
#   回春丹（商品 6）买 40 粒 —— 本道要请教约 28 次（force×10 + force_adv +
#     throwing×10~16 + throw_adv），每次扣 100 气，按「补满 → 学几次」循环；
#     买的是「尝试服用次数」，40 粒覆盖各轮补气
#   金创药（商品 5）买 2 包 —— 升级后把气血上限补满，让每轮能多学几次、少补几轮
#   棋子  （商品 10）买 1 盒 —— 投掷暗器：学高级投掷时必须手持它，偷袭也要它
#
# 判定：首粒断言本人专属成交回执，收尾断言背包里确有回春丹与棋子。
# 说明：各道采购量按本道所需核算，合计不超商人共享库存（回春丹 200 / 金创药 200）。

cmd /t 杂货老板
cmd /list
sleep 600

clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
sleep 600
repeat 39
cmd /buy 6
sleep 600
end

repeat 2
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
sleep 600
end

clear
cmd /buy 10
wait text 你从杂货老板那里买下了数量1的棋子 timeout=10000
sleep 600

wait item 回春丹 timeout=10000
wait item 棋子 timeout=10000
cmd /t 0

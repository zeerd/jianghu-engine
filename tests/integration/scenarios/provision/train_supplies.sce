# train_supplies.sce — L10（练功道）在山脚商人的补给采购
#
# 各道独立采购自己要用的物品，不争抢共享资源（§8.4③）：
#   回春丹（商品 6）买 30 粒 —— 本道请教约 24 次（force×10 + force_adv +
#     throwing×10 + throw_adv），每次扣 100 气，按「补满 → 学几次」循环
#     （与原 L3 combat_supplies 同款核算；L3 拆分后其采购瘦身为棋子+少量回春）
#   金创药（商品 5）买 2 包 —— levelup_heal 升级后把 eff_hp 缺口补满
#   棋子  （商品 10）买 1 盒 —— learn_throwing 的 throw_adv valid_learn 要求
#     「基本投掷 ≥10 且手持暗器」，必须 wield 它才教得进（片段内自 wield）
#
# 判定：首粒/首包断言本人专属成交回执，收尾断言背包里三样都在。
# 说明：商人库存全服共享且买空即下架——各道采购量按本道所需核算、
#   合计不超库存（回春丹 200 / 金创药 200 / 棋子 20）。

# 刚落地（登录/换房）：wait room 只确认 GOTOROOM 已处理，房间对象的 ADDOBJECT
# 是另一条报文、TCP 分段下可晚一拍到，而 /t 的英文 id 在客户端对象名录里解析——
# 先 wait object 再定向
wait object 杂货老板
cmd /t huo ji
cmd /list

# 每粒都等成交回执再发下一发：命令连发会打满服务端 20 槽的待处理队列
# （User.cpp push_command 回压，尾部直接丢弃），逐发 wait 让在途命令恒为 1，
# 且每粒的成交都成为断言——盲发只能证明「发过」，丢了几发都看不见。
repeat 30
clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
end

repeat 2
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
end

clear
cmd /buy 10
wait text 你从杂货老板那里买下了数量1的棋子 timeout=10000

wait item 回春丹 timeout=10000
wait item 金创药 timeout=10000
wait item 棋子 timeout=10000
cmd /t 0

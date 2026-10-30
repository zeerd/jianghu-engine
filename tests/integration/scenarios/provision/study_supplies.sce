# study_supplies.sce — L2（学艺道）在山脚商人的补给采购
#
# 各道独立采购自己要用的物品，不争抢共享资源：
#   回春丹（商品 6）买 30 粒 —— 每次请教扣 100 气（hp），按「补满 → 学几次」
#     循环服用；本道学 force/sword/literate + 两个高级共约 24 次请教
#   金创药（商品 5）买 2 包 —— 升级后回满气血
#   铁剑（商品 1）买 1 柄 —— sword_adv 的 valid_learn 要求「手持剑类武器」；
#     山脚木箱那把归 L1，本道必须自己买，否则学高级剑法必被拒
#   内功秘籍（商品 3）买 1 本 —— study 错误分支要按名定向到它
#
# 判定：首粒回春丹断言本人专属成交回执（证明买入链路真的生效），
#   收尾再断言背包里确实有回春丹、铁剑与内功秘籍——只发 /buy 不看回执等于没测。
#
# 说明：商人库存全服共享且买空即下架（CVendor::do_buy 的 m_amount--），
#   各道采购量按本道所需核算、合计不超库存（回春丹 200 / 金创药 200 / 铁剑 10）。

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
cmd /buy 1
wait text 你从杂货老板那里买下了数量1的铁剑 timeout=10000

clear
cmd /buy 3
wait text 你从杂货老板那里买下了数量1的内功秘籍 timeout=10000

wait item 回春丹 timeout=10000
wait item 铁剑 timeout=10000
wait item 内功秘籍 timeout=10000
cmd /t 0

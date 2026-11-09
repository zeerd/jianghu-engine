# social_supplies.sce — L4（社交道）在山脚商人的补给采购
#
#   蛇胆（商品 7）买 3 颗 —— 毒药/补药对照的中毒源（全服仅本道服用）：
#     1 颗走涂毒链+首次服下，另 2 颗供解毒链（雄黄专解、解毒丸通解各一次）
#   铁剑（商品 1）买 1 把 —— 涂毒要手持兵器（poison_base.do_mo 的武器前置）
#   金创药（商品 5）买 5 包 —— 中毒后循环补满气血上限（单次最多补 100，未必一次到位）
#     + 验证「不需要用」拒绝分支
#   回春丹（商品 6）买 8 粒 —— 中毒后回气血循环（同样可能要几粒）+ 「不需要用」拒绝分支
#   传送符（商品 8）买 1 张 —— 传送菜单与目的地选择
#   解毒丸（商品 15）买 1 粒、雄黄（商品 16）买 1 包 —— 解毒链的两样解药
#     （解毒丸清全身、雄黄专解蛇毒，见 item/sample_antidote.lua、
#       item/sample_xionghuang.lua；上架压在队尾，前面序号不动）
#
# 判定：首件断言本人专属成交回执，收尾断言背包里确有蛇胆、传送符、铁剑
#   与两样解药，并真的把铁剑持在右手（涂毒的武器前置）。

# 刚落地山脚：wait room 只确认进房，房间对象的 ADDOBJECT 推送要再晚一拍，
# 而 /t 的英文 id 在客户端对象名录里解析——先 wait object 再定向
wait object 杂货老板
cmd /t huo ji
cmd /list

clear
cmd /buy 7
wait text 你从杂货老板那里买下了数量1的蛇胆 timeout=10000

repeat 2
clear
cmd /buy 7
wait text 你从杂货老板那里买下了数量1的蛇胆 timeout=10000
end

repeat 5
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
end

repeat 8
clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
end

clear
cmd /buy 1
wait text 你从杂货老板那里买下了数量1的铁剑 timeout=10000

clear
cmd /buy 8
wait text 你从杂货老板那里买下了数量1的传送符 timeout=10000

clear
cmd /buy 15
wait text 你从杂货老板那里买下了数量1的解毒丸 timeout=10000

clear
cmd /buy 16
wait text 你从杂货老板那里买下了数量1的雄黄 timeout=10000

wait item 蛇胆 timeout=10000
wait item 铁剑 timeout=10000
wait item 传送符 timeout=10000
wait item 解毒丸 timeout=10000
wait item 雄黄 timeout=10000

# 持剑：涂毒（poison_base.do_mo）要求 query_weapon() 非空
cmd /t sample_sword_basic
clear
cmd /wield
wait wield 铁剑 右手 timeout=15000
cmd /t 0

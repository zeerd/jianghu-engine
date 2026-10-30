# combat_supplies.sce — L3（战斗道）在山脚商人的补给采购（N4 拆分后瘦身）
#
#   棋子  （商品 10）买 1 盒 —— sneak 的暗器（get_first_throwing_weapon 找
#     手持或背包；wield 后普通攻击也走 throwing 线，attack_type=throwing）
#   回春丹（商品 6）买 6 粒 —— 尸体腐烂段穿行保险 2 粒 + 山鸡战后残血缓冲；
#     N4 拆分后本道不再请教（learn 链迁 L10），原 40 粒需求随之消失
#
# 判定：首粒断言本人专属成交回执，收尾断言背包里确有棋子与回春丹。
# 说明：各道采购量按本道所需核算，合计不超商人共享库存（回春丹 200 / 棋子 20）。

# 刚落地（登录/换房）：wait room 只确认 GOTOROOM 已处理，房间对象的 ADDOBJECT
# 是另一条报文、TCP 分段下可晚一拍到，而 /t 的英文 id 在客户端对象名录里解析——
# 先 wait object 再定向
wait object 杂货老板
cmd /t huo ji
cmd /list

repeat 6
clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
end

clear
cmd /buy 10
wait text 你从杂货老板那里买下了数量1的棋子 timeout=10000

wait item 回春丹 timeout=10000
wait item 棋子 timeout=10000

# wield 棋子：sneak 的 get_first_throwing_weapon 优先取手持武器；更重要的是
# 战斗里 wield 决定 attack_type=throwing（否则空手走 unarmed、本道无此技能）
cmd /t sample_chess
clear
cmd /wield
wait text 从怀中夹出一枚棋子 timeout=10000
wait wield 棋子 右手 timeout=15000
cmd /t 0

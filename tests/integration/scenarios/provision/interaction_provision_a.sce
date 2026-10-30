# interaction_provision_a.sce — L5（配对互动 A 道）在山脚商人的补给采购
#
#   金创药（商品 5）买 1 包 —— 玩家间赠送（give/do_give 用户分支）的赠品，
#     由 A 赠予 L6 道，不依赖任何钉给唯一道的稀缺资源（§8.4③ / §8.6⑤）。
#
# 判定：断言本人版成交回执 + 收尾断言背包里确有金创药。
# 说明：本道起始房间即「青云山山脚」（杂货老板在此），采购后步行去会合房间。

# 刚落地（登录/换房）：wait room 只确认 GOTOROOM 已处理，房间对象的 ADDOBJECT
# 是另一条报文、TCP 分段下可晚一拍到，而 /t 的英文 id 在客户端对象名录里解析——
# 先 wait object 再定向，否则 /t 落空（target=0）后续 /buy 全部静默失败
wait object 杂货老板
cmd /t huo ji
cmd /list

clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
wait item 金创药 timeout=10000
cmd /t 0

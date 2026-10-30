# interaction_provision_a.sce — L5（配对互动 A 道）在山脚商人的补给采购
#
#   金创药（商品 5）买 1 包 —— 玩家间赠送（give/do_give 用户分支）的赠品，
#     由 A 赠予 L6 道，不依赖任何钉给唯一道的稀缺资源（§8.4③ / §8.6⑤）。
#
# 判定：断言本人版成交回执 + 收尾断言背包里确有金创药。
# 说明：本道起始房间即「青云山山脚」（杂货老板在此），采购后步行去会合房间。

cmd /t huo ji
cmd /list

clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
wait item 金创药 timeout=10000
cmd /t 0

# give_b.sce — 玩家间赠送（B 方：断言金创药真实到账背包）
#
# 与 give_a.sce 配对并行。以背包 item 条件断言（只与本人相关、最可靠，§8.6①），
#   不依赖 message_vision 房间版回执。

wait item 金创药 timeout=20000
sleep 400

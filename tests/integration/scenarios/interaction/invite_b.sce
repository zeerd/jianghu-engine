# invite_b.sce — 做客邀请（B 方：接受 A 的做客邀请）
#
# 与 invite_a.sce 配对并行。A 需先完成置业再步行回茶棚邀请，耗时较长，
#   故 B 的 `wait menu` 取宽 timeout。B 一直在茶棚等待（§8.6④ 超时即疑似
#   对端未到位——先查 A 道日志）。

wait menu timeout=60000
menu 接受
wait text 你接受了邀请 timeout=15000

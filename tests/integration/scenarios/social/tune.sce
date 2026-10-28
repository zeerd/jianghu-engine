# tune.sce — /tune 两分支：关一次成功、再关落到 else 分支
#
# 位置：channel_and_banword 之后（新号 channels=[chat,rumor]）。
# ⚠ 时序纪律（见 social/account.sce 头注释）：本片段无 DB 写，安全放道尾。
#   关掉 chat 不影响本道后续断言（本道此后不再有频道消息断言）。

clear
cmd /tune chat
wait text 关闭chat频道 timeout=10000
sleep 400

clear
cmd /tune chat
wait text 要打开某个频道只要用该频道说话即可 timeout=10000
sleep 400

# tell_a.sce — 私聊与屏蔽（A 方：发起悄悄话，覆盖 tell 成功/被拒/恢复三态）
#
# 与 tell_b.sce 配对并行。免竞态握手（refuse_tell 只拦 A→B 的 tell 通道，
#   故用 B→A 的 tell 与 A→B 的房间版 say 作为信号，方向互不干扰）：
#   1) A 告诉 B（成功）  2) B 屏蔽后回执「已屏蔽」（B→A tell）
#   3) A 再告诉 B（被「不接受你的悄悄话」拒绝）  4) A 房间版 say「解除屏蔽」通知 B
#   5) B 恢复后回执「已恢复」（B→A tell）  6) A 再告诉 B（成功）

# 1) A 私聊 B（成功）
cmd /t $peer_acct
cmd /tell 悄悄话甲
wait text 你告诉 timeout=10000

# 2) 等 B 屏蔽后回执
wait text 已屏蔽 timeout=20000

# 3) A 再私聊 B（被屏蔽拒绝）
cmd /tell 悄悄话乙
wait text 不接受你的悄悄话 timeout=10000

# 4) A 房间版 say 通知 B 解除（refuse_tell 不拦 say/chat）
cmd /say 解除屏蔽

# 5) 等 B 恢复后回执
wait text 已恢复 timeout=20000

# 6) A 再私聊 B（成功）——clear 清掉 msg1 的旧「你告诉」，确保断的是 msg3 真成功
clear
cmd /tell 悄悄话丙
wait text 你告诉 timeout=10000
cmd /t 0

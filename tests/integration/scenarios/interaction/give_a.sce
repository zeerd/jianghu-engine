# give_a.sce — 玩家间赠送（A 方：do_give 用户分支，把金创药赠予 B）
#
# 与 give_b.sce 配对并行。`/t B` + `/give 金创药` 走 CLI give 助手 →
#   服务端 do_give（玩家收款无阻拦，message_vision + move）。A 侧本人版
#   「你给<peer>数量1的金创药」；B 侧以背包 item 条件断言物品真实到账（§8.6①）。

cmd /t $peer_acct
cmd /give 金创药
wait text 给$peer数量 timeout=15000
cmd /t 0

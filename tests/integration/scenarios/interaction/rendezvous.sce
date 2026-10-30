# rendezvous.sce — 配对互动道（L5/L6）的会合片段
#
# 依赖变量：
#   peer : 配对道的角色名（run-integration-tests.ps1 在注册阶段注入，
#          如 L5 的 vars 里 peer=e2e<runhex>6、L6 里 peer=e2e<runhex>5）
#
# 会合机制（松同步，无 CLI 屏障动词）：
#   双方都从起始房间「青云山山脚」步行到会合房间，先 `wait room` 确认自己
#   到位，再 `wait object $peer` 等对方出现。服务端 on_enter 会给房内已有
#   玩家双向推 ADDOBJECT（ContainerMove.cpp），所以**后到者进门的瞬间双方
#   同时满足条件**，没有先后竞态；先到者等的是后到者的进门推送。
#
# 会合房间选「山脚茶棚」的理由（§8.6 ③ 会合房间避开其他道动线）：
#   lua-sample 可行走房间全部在某道动线上，茶棚暴露面最小——仅 L1
#   tea_fill_drink.sce 短暂经过，且其断言全是本人版文本/私发通道，
#   与本房间是否有其他玩家互不干扰；本片段也不做任何资源操作。
#
# 超时口径：对方若因登录失败/中途失败停摆，本片段的 `wait object $peer`
# 会先到期——排查时应先看对端道的日志（疑似对端未到位，见 §9）。
# timeout 取宽（180s）：A 侧会合前要走 pk_prep（起步历练、买剑装备、冲 10 级
# 约 30s），先到者等后到者；宽余量同时吸收对端登录/数据库抖动。

cmd /go 山脚茶棚
wait room 山脚茶棚 timeout=30000
wait object $peer timeout=180000

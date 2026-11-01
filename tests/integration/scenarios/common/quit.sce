# quit.sce — 道收口：主动退出（必须是整道最后一个 include）
#
# quit 触发服务端 DESTRUCT 断连，其后任何 wait 都会因 "server closed the connection"
# 判失败（剧本器每步开始前先查 server_closed），故本片段之后不再有任何片段。
# quit 的可观测结果即「连接正常收尾」：报告 pass + CLI 退出码 0 收口；
# 前置条件由 wait battle_end 兜底（quit 的两个拒绝分支是战斗中/忙碌，
# 本道全程无战斗、片尾无 busy）。
#
# ⚠ 当前只有 L1 挂本片段（最速道，约 25~30s 收尾）而不是 L4——quit 触发的
#   save_to_server 是全量落库，会把单线程工作线程停顿数秒；L4 若在道尾 quit，
#   正好撞上 L3 的 heal→busy 命令边界（run2 红的根因）。L1 收尾时
#   L2 的首个 heal 边界约 48s、L3 约 150s 都还没到，全道无任何 busy 边界。

wait battle_end
cmd /quit

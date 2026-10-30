# treebox.sce — 上山顶：等随机树箱刷出 → 取经验果 → 服下（内力上限 +100）
#
# 覆盖：sample_guide.lua 的 do_refresh 随机刷箱（1/10 每 5 秒掷一次，Set_Box 后
#   停止重排）、CRoom::Add_Box 与容器取物、sample_xp.lua 的经验果 do_use
#   （combat_exp +100000、max_mp +100、FlushMyInfo）。
#
# ⚠ 并行隔离：树箱全服只刷一只、经验果只有一粒（random/tree 置位后不再重排，
#   箱子也不随 CRoom::reset 恢复），故取果段只由 L3 一道执行——这 +100 上限
#   也只有它用得上（偷袭要当前内力 ≥100）。
#
# 判定：
#   · 山顶场景描述私发；`wait object` / `wait menu` 都是每客户端独立状态；
#   · 断言背包里真多了一粒经验果，再断言本人版服用回执「你吃下一粒经验果」；
#   · 服下后 FlushMyInfo 会推送 max_mp，用状态断言直接验证上限确实 +100
#     （上限从第一轮打坐的 1 抬到 101）。

cmd /go 青云山山顶
wait room 青云山山顶

clear
cmd /look
wait text 山顶前方有一座山寨 timeout=10000

# 随机树箱：概率性刷新（脚本 1/10 每 5 秒重掷，刷出即停）
wait object 一个奇怪的树 timeout=240000
cmd /t 0
cmd /t tree
cmd /look
wait menu
menu 经验果
wait item 经验果 timeout=10000

cmd /t jing yan dan
clear
cmd /use
wait text 你吃下一粒经验果 timeout=10000
wait attr max_mp >= 100 timeout=10000
# 经验果的 do_use 会 start_busy(2)（sample_xp.lua），后续片段的打坐准入拒 busy，
# 心跳拍计时只能等时间过拍
sleep 3200
cmd .info
cmd /t 0

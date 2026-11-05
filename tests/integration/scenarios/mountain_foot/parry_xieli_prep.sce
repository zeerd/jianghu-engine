# parry_xieli_prep.sce — 卸力（高级格挡绝招）前置：供能 → 激发 → 施放时机负支
#
# 教材：lua-sample/skill/parry_adv.lua 的 do_xieli / xieli_end——战斗中起势叠
#   apply/defense，用 call_out_combat 挂 delay=0 的哨兵（心跳不走表），只等
#   这场战斗结束触发收势。本段只做**战斗外**的部分：
#   · 供能与激发；
#   · 「脱战不能起势」这条门——状态的解除时机就是战斗结束，脱战起势没有解除
#     时机（notify_fail 私发，仅本人可见）。
# 起势 / 持续 / 收势三步都要真开战，归 road_rogue 段的死战，见该段
#   「卸力起势」「卸力收势」两小节与头注。
#
# 为什么拆两处（不是把战斗搬过来）：
#   ① 供能请求要发得早——L9 服务链按到达序消费，本请求是全链最早一路
#      （见 wizard/service.sce 链首段），晚发会滞留别人的回执；
#   ② 起势只能在战斗中，而本道唯一归己的战斗对手是山道强人——它的棋子是
#      一次性消耗暗器且 road_rogue 段要断「扬起棋子」，多开一场就会把它吃掉
#      （§8.4③ 稀缺资源唯一归属）；所以起势长在 road_rogue **已经开打**、
#      且棋子断言已过的那场里。
#
# 供能（§2.6 协议）：样例世界没有格挡师父教本道、打坐攒 mp 远超 E2E 时长预算，
#   故技能与内力由 L9 直设。parry_adv 给到师父级 500：施法 AP 费 =
#   ap×(120 − 技能/4 − 等级/2)/100 在 500 级下为负、引擎夹到 1 点，开战入队
#   那一刻的 AP（dex/2 = 10，join_team 定值）即够起势——起势不必等拍攒 AP，
#   也就没有可 wait 的拍数状态（§8.1：不拿 sleep 赌拍）。
#   /enable parry parry_adv 是用户命令、本道自做（非 force 分支不清 mp）。

# --- 供能：技能 + 内力（发完即进宽 timeout 等回执，回执毫秒级）--------------
cmd /t 1
cmd /tell 卸力供能请求，卸力供能账号是 $acct
wait text 卸力供能完毕 timeout=300000
wait attr mp >= 100 timeout=10000
cmd /t 0

# --- 激发：用户命令自做，回执 Ok. ---------------------------------------------
clear
cmd /enable parry parry_adv
wait text Ok. timeout=10000

# --- 负支：脱战起势被拒（do_xieli 的 is_fighting 门；此时无人交手）------------
clear
cmd /cast parry xieli
wait text 只在与人交手 timeout=10000
cmd /t 0

# arena.sce — 山脚演武场：脚本驱动引擎战斗房的完整一条龙
#
# 覆盖（§6 FightRoom 认领段重开，12 条收益）：
#   lua 侧建房链（sample_arena_master.lua do_talk）：
#     l_is_fight_room（验型）→ l_can_join（验名额）→ l_set_single（单人比试）
#     → l_join_team ×2（双方入队）→ l_begin_fight（开打）
#   战况查询（战报四连，全部私发给挑战者）：
#     l_get_fight_team_num（A/B 两队人数，内含 C++ get_team_count）
#     l_get_team_list（B 队名册）→ l_select_member（A 队点名）
#   脚本收场（yield callout，数拍后）：
#     l_get_fight_team_char（本队头名）→ l_leave_team（退己方队）
#     → l_end_fight（收比武房，房内角色移回演武场）
#   C++ get_team_count 另经 l_get_fight_team_char 的 get_team_list(team)
#   默认参数路再走一遍。
#
# 与既有比试（interaction/fight_a.sce 的 /fight C++ 路径）分工：那里覆盖
# do_bishi 的 C++ 建房，这里覆盖教头脚本逐绑定自建——两条路殊途同归。
#
# 判定：
#   /ask 比武 → 私发战报「演武比试开打…」（say 第二参收件人，§8.6①）
#   入队即置战斗态 → wait battle
#   教头 6 心跳拍后点到为止 → 房间版「点到为止」（battle 房内只有本道，
#     且句含教头自称）→ End_Fight 播报「演武比试结束了。」（该句只有
#     收比武房会发，全服唯一触发源是本段）
#   wait battle_end + wait room 演武场（end_fight 把房内角色移回环境）
#
# 时序：yield 是心跳拍 callout（§8.1 等心跳只能 wait 收敛），timeout 30s 兜底。
# 并行说明：演武场只有本道会去（§8.4③），战报私发、收场播报都在
#   per-instance 战斗房/本房间内，无跨道串扰。
# 位置：本段排在 interaction/spectate.sce 之后——spectate 段全程在山脚茶棚
#   收 L5 信号且段内有 clear，先把信号消费干净再开比，不给 §8.6⑦ 的
#   clear 吞跨道信号留窗口。

# 起点是 spectate 收尾的山脚茶棚——茶棚只有回山脚的一扇门，去演武场
# 须先回山脚再走（/go 只走直达门，同 tea_fill_drink 的跨门教训）
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚 timeout=10000
cmd /go 山脚演武场
wait room 山脚演武场 timeout=10000
wait object 演武教头 timeout=10000

cmd /t yan wu
clear
cmd /ask 比武
wait text 演武比试开打 timeout=10000
wait battle timeout=10000

# 教头数拍后点到为止：收场三连（战报收尾 + 退队 + 收房）全脚本驱动
wait text 点到为止 timeout=30000
wait text 演武比试结束了 timeout=10000
wait battle_end timeout=10000
wait room 山脚演武场 timeout=10000
cmd /t 0

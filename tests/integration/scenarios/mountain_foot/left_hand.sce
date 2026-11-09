# left_hand.sce — 左手双持：匕首（轻便可连发）/ 短剑（势沉攻击高）
#
# 覆盖（item/sample_dagger.lua、item/sample_short_sword.lua）：
#   · wield_position=WIELD_LEFT_HAND 的落槽语义——/wield 无槽参数，
#     槽位由物品字段决定，回执用 CLI 的 wield 条件断言「左手」；
#   · 同槽换装必须先卸后装（直接换会被「已有装备」拒绝）；
#   · can_combo 判据在匕首身上——本段收尾把匕首装回左手，供 road_rogue
#     死战触发 data/hooks.lua 的「左手连击」（断言私发文案见该段）。
#
# 判定：wield/unwield 回执均为本人版 message_vision（wield_msg/unwield_msg），
#   槽位用状态断言（wait wield <名> 左手），与 §8.4① 一致。

wait object 杂货老板
cmd /t huo ji
clear
cmd /buy 17
wait text 你从杂货老板那里买下了数量1的匕首 timeout=10000
clear
cmd /buy 18
wait text 你从杂货老板那里买下了数量1的短剑 timeout=10000

# 匕首落左手
cmd /t bi shou
clear
cmd /wield
wait text 匕首滑入左手 timeout=10000
wait wield 匕首 左手 timeout=10000

# 同槽换短剑：先卸匕首再装
cmd /unwield
wait text 匕首收回袖中 timeout=10000
cmd /t duan jian
clear
cmd /wield
wait text 短剑压入左手 timeout=10000
wait wield 短剑 左手 timeout=10000

# 换回匕首参战（连击判据 can_combo 在它身上）
cmd /unwield
wait text 短剑插回鞘中 timeout=10000
cmd /t bi shou
clear
cmd /wield
wait text 匕首滑入左手 timeout=10000
wait wield 匕首 左手 timeout=10000
cmd /t 0

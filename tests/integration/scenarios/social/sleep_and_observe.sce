# sleep_and_observe.sce — 打坐恢复（睡眠场所）→ 观察师父（同门关系分支）
#
# 覆盖：CCmdStandard::Sleep 的 sleep_room 判定与入睡/醒来播报、
#   CChar::do_look 的 family_des 同门关系分支（「是你的师父」）与 eff_status_msg。
# 判定：
#   · 「你一觉醒来」必须取本人版——醒来同时有房间版（渲染成「<名字>一觉醒来」）
#     和本人版（渲染成「你一醒来」），而 L2 也在这间睡眠场所睡觉，只认本人版
#     才不会被它提前满足（被提前满足会带着睡意往下走，后续命令全被拒）；
#   · 「开始睡觉」由 tell_object 私发，房间版文案里没有这几个字；
#   · 「是你的师父」只有本道会去定向观察青云子。
#
# 前置：common/apprentice.sce 已把本道带到半山腰并完成拜师（观察师父要认门派）。

clear
cmd /sleep
wait text 开始睡觉 timeout=5000
wait text 你一觉醒来 timeout=120000
cmd .info

wait object 青云子
cmd /t 青云子
clear
cmd /look
wait text 是你的师父 timeout=10000
sleep 500
cmd /t 0

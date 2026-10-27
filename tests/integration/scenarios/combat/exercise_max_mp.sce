# exercise_max_mp.sce — 打坐机制：内力上限 0→1 的那条分支
#
# 覆盖：CCmdStandard::exercise 的准入（force 必须已 enable / 非安全区）、
#   exercising 的逐拍换气（每拍花 1 气换 1 内力，累加进**当前 mp**、不动上限）、
#   以及收工时「当前 mp ≥ max_mp×2 就把上限抬 1 点、并把 mp 设成新上限」——
#   上限一次只抬 1 点，`set exercise 10` 不会让上限变成 10；正因收工把 mp
#   重设成上限，这里收工后 mp 正好等于 1。
# 判定：
#   · 「你坐下来运气用功」「你的内力增加了」都是 tell_object 私发；
#   · 「运功完毕」走 message_channel，它对本人渲染成「你运功完毕」、对同房间
#     别人渲染成「<名字>运功完毕」——只认「你」字开头的本人版，半山腰另外
#     两道即使同时在场也不会误满足；
#   · 打坐抬的是**内力上限 max_mp**（收工那句「内力增加了」伴随的就是
#     max_mp += 1，并把当前 mp 重设成新上限），所以状态断言打 max_mp 0→1，
#     而不是当前 mp——当前 mp 在收工时被重设，本就等于上限。
#
# 说明：半山腰未设 no_fight（山脚设了），所以打坐只能在此处进行。

clear
cmd /set exercise 10
wait text 你设置环境变量 timeout=10000
sleep 400

clear
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
wait text 你运功完毕 timeout=60000
wait text 内力增加了 timeout=10000
wait attr max_mp >= 1 timeout=10000
sleep 400

# halt_exercise.sce — 打坐中途 /halt 中断（interrupt_me → halt_exercise 回调）
#
# 覆盖：CCmdStandard::halt 的非战斗分支（is_busy → interrupt_me(me,"halt")）
#   与 CCmdStandard::halt_exercise 的 how=="halt" 分支。
#
# 判定：
#   · 中断回执「你把正在运行的真气强行压回丹田，站了起来。」是 message_vision
#     本人版（$N→你）；半山腰同房还有 L2/L4，房间版渲染成「<名字>把…」，
#     不会串到本道断言，本道也不会被别人满足（本片段只有本道会喊 /halt）；
#   · `wait attr max_mp <= 1` 钉住「打坐没跑完」——exercise 正常收工时
#     mp ≥ max_mp×2 会把上限抬 1（见 exercise_max_mp 片段），中止发生在
#     半途则上限停在 1；若 /halt 失败打坐跑完，此处即红。
#
# 说明：/set exercise 10 的 env 值跨片段保留，这里重设一遍是为了片段自包含；
#   中止后 pending/exercising 与 busy 均被回调清零，不污染后续 learn_throwing。

clear
cmd /set exercise 10
wait text 你设置环境变量 timeout=10000

clear
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
# 起手回执一到、心跳尚未跑完打坐，立刻 /halt 打断
cmd /halt
wait text 你把正在运行的真气强行压回丹田 timeout=10000

# 中止后未收工 → 抬上限的分支不触发，max_mp 仍是 1
wait attr max_mp <= 1 timeout=10000

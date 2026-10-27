# force_for_exercise.sce — 内功链：练到高级并激发（exercise 的前置）
#
# 与 L2 的 skill/learn_force.sce 同源，但这条是战斗道的**前置**：exercise 命令
# 要求 force 已被 enable（CmdStandardLife.cpp:311），而基本技能不能自我映射
# （enable X X 回「是基础，不需要 enable」），所以必须学到 force_adv 再激发。
#
# 判定：私发的「进步了」+ 本人版补气双分支，每次迭代先 clear。
# 说明：请教一次扣 100 气，按「学 3 次 → 补气一轮」循环；升级与金创药已由
#   combat/levelup_heal.sce 把气血上限顶到 489，一轮 4 粒补气可支撑下一轮。

cmd /t 青云子

repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600

clear
cmd /learn force_adv
wait text 进步了 timeout=10000
sleep 600

# 激发高级内功：把 mp 清零（正是 exercise 0→1 那条分支的前提）
clear
cmd /enable force force_adv
wait text 内力必须重新修炼 timeout=10000
sleep 600
cmd /t 0

# learn_force.sce — 基本内功逐级请教到 10 级，再越过 valid_learn 门槛学高级内功
#
# 覆盖：CmdStandardMaster::learn 的请教链路（improve_skill 每次恰好 +1 级）、
#   skill_need_points / query_valid_exp 的经验校验、
#   lua-sample/skill/force_adv.lua 的 valid_learn（基本内功火候不够 → 拒绝）。
# 判定：每次请教都断言私发的「进步了」；补气断言本人版双分支——
#   「你吃下一粒回春丹」（服下）或「你现在不需要用回春丹」（已满，notify_fail
#   仅本人可见），两者都只有本人收得到，同房间别的账号不会串进来。
#
# 说明：请教一次扣 100 气（hp），按「补满 → 学几次」循环；每次迭代先 clear，
#   保证本轮的等待只看本轮的回执，不会被上一轮的旧输出误满足。

cmd /t qing yun zi

repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
end

repeat 4
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t qing yun zi
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
end

repeat 4
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t qing yun zi
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
end

repeat 4
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t qing yun zi
clear
cmd /learn force_adv
wait text 进步了 timeout=10000
cmd /t 0

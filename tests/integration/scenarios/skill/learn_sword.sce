# learn_sword.sce — 基本剑法逐级请教到 11 级，再学高级剑法
#
# 覆盖：与 learn_force.sce 同一条请教链路，走 sword/sword_adv 这条技能线，
#   以及 lua-sample/skill/sword_adv.lua 的 valid_learn。
# 判定与说明同 learn_force.sce：私发的「进步了」+ 本人版补气双分支，
#   每次迭代先 clear。

cmd /t 青云子

repeat 2
clear
cmd /learn sword
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
cmd /learn sword
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
cmd /learn sword
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

repeat 2
cmd /t 青云子
clear
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
end

# sword_adv 的 valid_learn 除「基本剑术 ≥10」外还要**手持剑类武器**
# （sword_adv.lua:63-66）。山脚木箱那把铁剑归 L1，本道的剑是自己采购的
# （provision/study_supplies.sce），所以请教高级剑法前必须先装备上。
# 注意装备会把目标切成剑，教高级剑法前必须切回青云子——否则 get_room_char
# 拿到的是背包里的物品，回「你要向谁请教？」。
cmd /t 铁剑
clear
cmd /wield
wait text 抽出一柄 timeout=10000
wait wield 铁剑 右手 timeout=10000
sleep 600

cmd /t 青云子
clear
cmd /learn sword_adv
wait text 进步了 timeout=10000
sleep 600
cmd /t 0

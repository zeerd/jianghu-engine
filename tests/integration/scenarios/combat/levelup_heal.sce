# levelup_heal.sce — 升级 ×2 → 金创药补「升级造成的有效上限缺口」
#
# 覆盖：CCmdUser::upgrade、金创药的 receive_curing（抬 eff_hp）分支。
#
# 为什么必须先升级：level2skill 对 5 级以下给出 (lv+1)*4，1 级角色的技能上限
#   只有 8（CharLevel.cpp），而 learn 对 martial 技能有硬门槛——本道要把
#   force / throwing 请教到 10 以上，不升到 3 级（上限 16）就会卡住。
#
# 判定：数值断言，不只看「把药吃下去了」。角色由 register.sce 定死
#   str/con/int/dex = 21、config 的 start_age = 16，且 config 的 force_add_hp = 0，故
#     max_hp = 300 + level × con × 3 + (16−14) × con → 1 级 405、3 级 531；
#     而升级只改 max_hp、不动 eff_hp，升完必然出现「531 / 405」这个缺口。
#   · 升完断言 max_hp 抬到 531、同时 eff_hp 仍 < 531 —— 钉住所要补的缺口；
#   · 金创药断言本人版回执；
#   · 再断言 eff_hp >= 531 —— 缺口确实被补上了。

repeat 2
clear
cmd /upgrade
wait text 升级成功 timeout=10000
sleep 600
end

# 升级抬高了最大上限，有效上限仍停在升级前的 405
wait attr max_hp >= 531 timeout=10000
wait attr eff_hp < 531 timeout=10000

repeat 2
cmd /t 金创药
clear
cmd /use
wait text 你吃下一包金创药 timeout=10000
sleep 5500
end

# 有效上限追平最大上限：升级造成的缺口被补满
wait attr eff_hp >= 531 timeout=10000
cmd /t 0

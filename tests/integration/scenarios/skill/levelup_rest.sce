# levelup_rest.sce — 升级 → 金创药补有效上限 → 睡觉补当前气血
#
# 覆盖：CCmdUser::upgrade、金创药的 receive_curing（抬 eff_hp）、
#   CCmdStandard::Sleep 的入睡与醒来恢复（hp 补到 eff_hp）。
#
# 判定：三个量分别钉住，缺一个都不知道补的是什么。
#   角色由 register.sce 定死 str/con/int/dex = 21、config 的 start_age = 16，
#   且 config 的 force_add_hp = 0，故 max_hp = 300 + level × con × 3 + (16−14) × con：
#     1 级 405 → 升到 3 级 531；升级只改 max_hp，eff_hp 仍停在 405。
#   · 升完：max_hp >= 531、eff_hp < 531 —— 金创药要补的缺口存在；
#   · 吃完金创药：eff_hp >= 531 —— 「金创药补 eff_hp」成立；
#   · 睡前 hp < 531（学艺扣过的旧值）→ 睡醒 hp >= 531 —— 「睡觉补 hp」成立。
#
# 说明：半山腰是 sleep_room（room/sample_mountain_mid.lua），本道在此入睡。

repeat 2
clear
cmd /upgrade
wait text 升级成功 timeout=10000
end

# 升级抬高最大上限，有效上限滞后 —— 钉住这个缺口
wait attr max_hp >= 531 timeout=10000
wait attr eff_hp < 531 timeout=10000

repeat 2
cmd /t jin chuang yao
clear
cmd /use
wait text 你吃下一包金创药 timeout=10000
sleep 3200
end

# 金创药补的是有效上限 eff_hp
wait attr eff_hp >= 531 timeout=10000

# 睡前的「hp 不满」已由 skill/learn_basic.sce 在请教完立刻断过（3 次共扣 300 气，
# 到这里没有任何步骤会回血——升级只改 max_hp、金创药只抬 eff_hp），
# 这里只等睡醒把 hp 补到有效上限，正是「睡觉补 hp」的验证点。
cmd /t 0
clear
cmd /sleep
wait text 开始睡觉 timeout=5000
wait text 你一觉醒来 timeout=120000
wait attr hp >= 531 timeout=10000
clear
cmd .info

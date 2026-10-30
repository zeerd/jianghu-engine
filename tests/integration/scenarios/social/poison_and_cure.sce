# poison_and_cure.sce — 毒药/补药对照 + 状态（condition）生命周期 → 传送符换场景
#
# 覆盖：
#   item/sample_poison.lua 的即时双损伤（hp 扣 40% 上限、eff_hp 扣 10% 上限）
#   condition/sample_poison.lua：apply_condition 挂载 → CLuaCondition 逐拍
#     update_condition 递减 → 归零解除，三段播报依次命中
#   receive_curing / receive_healing 两条回复路径（金创药抬上限、回春丹回当前）
#   item/sample_teleport.lua 的传送菜单与目的地选择
#
# 判定：**吃药不看「把药吃下去了」，看数值真的回来了**。
#   本道不升级，register 又定死 str/con/int/dex = 21、config 的 start_age = 16，故
#   max_hp 恒为 300 + 1 × 21 × 3 + (16−14) × 21 = 405（age>14 走
#   base + (age−14)×con 阶梯，见 SetupChar.cpp setup_human_max_hp），
#   两段伤害与回补的判定（数值不写死，靠「服用直到拒绝」收敛）：
#     中毒后：hp 被打残 < 405、eff_hp 再扣上限 10%（40）        → 都 < 405
#     金创药：补**气血上限**，单次最多补 100；eff_hp 起点还可能
#             滞后于 max_hp（注册/升级时序），故循环服用直到拒绝
#             （拒绝 ⟺ eff_hp 已到 max_hp）        → eff_hp ≥ 405
#     回春丹：补**当前气血**、封顶到 eff_hp，同样循环服用直到拒绝
#                                          → hp ≥ 405
#     再用被拒：eff_hp 已 = max_hp、hp 已 = eff_hp          → 两条拒绝分支成立
#   另：hp/eff_hp 的本人版回执取「你…」，与 L2/L3 同在半山腰吃药也不会被串；
#   断肠散只由本道购买与服用，其房间版播报不会被别的账号触发。

# 先钉住百分比计算的基数，基线一漂后面所有断言都失去意义
wait attr max_hp == 405 timeout=10000

cmd /t sample_poison
clear
cmd /use
wait text 毒性发作了 timeout=10000
wait text 毒性渗入了你的体内 timeout=10000
wait text 毒性一阵阵发作 timeout=60000
wait text 毒性渐渐散去 timeout=90000

# 两段伤害都已落地：当前气血与有效上限都被压下去了
wait attr hp < 405 timeout=10000
wait attr eff_hp < 405 timeout=10000

# 金创药补的是**气血上限**（eff_hp）：单次最多补 100，而 eff_hp 起点可能还
# 滞后于 max_hp（注册/升级时序，实测 342），断肠散再扣 10%（40）→ 一次未必
# 补满（实测 302 → 402）。故多来几包直到拒绝（拒绝 ⟺ eff_hp 已到 max_hp），
# 补满后再断言 405 才站得住。
repeat 4
clear
cmd /t jin chuang yao
cmd /use
wait text 你吃下一包金创药|你现在不需要用金创药 timeout=10000
sleep 3200
end
wait attr eff_hp >= 405 timeout=10000

# 回春丹补的是**当前气血**（hp）：补到 hp = eff_hp；毒伤深浅随心跳时序浮动，
# 故按「重复服用直到补满」的循环写，而写死粒数
repeat 5
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end
wait attr hp >= 405 timeout=10000

# 上限与当前都已补满 → 两条拒绝分支才成立，此时的拒绝才证明「补到位了」
cmd /t jin chuang yao
clear
cmd /use
wait text 不需要用金创药 timeout=10000
wait attr eff_hp >= 405 timeout=10000

cmd /t hui chun dan
clear
cmd /use
wait text 不需要用回春丹 timeout=10000
wait attr hp >= 405 timeout=10000

# ---------------------------------------------------------------------------
# 传送符：look 弹使用菜单 → 选目的地（半山腰 → 山脚，衔接后续生活段）
# ---------------------------------------------------------------------------
cmd /t chuan song fu
cmd /look
wait menu
menu 使用传送符
wait menu
menu 传送至「山脚」
wait room 青云山山脚
cmd .info
cmd /t 0

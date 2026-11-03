# poison_and_cure.sce — 毒药/补药对照 + 状态（condition）生命周期 → 传送符换场景
#
# 覆盖：
#   item/sample_shedan.lua 的即时双损伤（hp 扣 40% 上限、eff_hp 扣 10% 上限）
#   condition/snake_poison.lua：apply_condition 挂载 → CLuaCondition 逐拍
#     update_condition 递减 → 归零解除，三段播报依次命中
#   poison_base 的涂毒面：do_look（剩余可用次数）→ do_mo 成功支挂 poison/modu
#     → 二涂被「已涂过」拒绝（拒绝条件读的就是 poison/modu）→ 毒效到点自动解除
#     涂毒三前置（毒术 / 手持兵器 / 内力≥30）：前两项由巫师供能与本道自持铁剑
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
#   服毒只发生在本道（蛇胆由本道购买并服下，L3 捡到的那颗原样留着），
#   其房间版播报不会被别的账号触发。

# 先钉住百分比计算的基数，基线一漂后面所有断言都失去意义
wait attr max_hp == 405 timeout=10000

# --- 涂毒链（poison_base.do_mo 成功支：毒术 + 手持兵器 + 内力≥30，缺一不可）---
# 毒术与内力由巫师供能；从请求到回执之间全程不 clear（§8.6⑦，回执在途）。
# 兵器是 provision 段自购自持的铁剑；30 就是 do_mo 的内力门槛
cmd /t 1
cmd /tell 毒术供能请求，毒术供能账号是 $acct
wait text 毒术供能完毕 timeout=300000
wait attr mp >= 30 timeout=10000

cmd /t she dan
clear
# 观察面走 poison_base.do_look：显剩余可用次数并弹涂毒菜单（本道已获毒术
# 且持有此物，菜单分支成立）；后续 /tumo 是非数字输入，先取消菜单再当命令发
cmd /look
wait text 还可以被使用3次 timeout=10000

# 首涂成功（私发回执）：三项前置同时成立才走得到这一支
cmd /tumo
wait text 你小心翼翼地将蛇胆涂抹到了 timeout=10000

# 二涂必被拒——拒绝条件读的正是 query_temp("poison/modu")，
# 能拒绝就证明涂毒状态真的挂在身上（§8.4① 用状态而非回执）
cmd /tumo
wait text 你的武器上已经涂过蛇胆了 timeout=10000

# 涂毒时效：do_mo 以毒术等级为拍数登记 remove_effect 回调，到点自解除
wait text 涂在武器上的毒药失效了 timeout=30000

cmd /use
wait text 毒性发作了 timeout=10000
wait text 毒性渗入了你的体内 timeout=10000
wait text 毒性一阵阵发作 timeout=60000
wait text 毒性渐渐散去 timeout=90000

# 两段伤害都已落地：当前气血与有效上限都被压下去了
wait attr hp < 405 timeout=10000
wait attr eff_hp < 405 timeout=10000

# 金创药补的是**气血上限**（eff_hp）：单次最多补 100，而 eff_hp 起点可能还
# 滞后于 max_hp（注册/升级时序，实测 342），蛇胆再扣 10%（40）→ 一次未必
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

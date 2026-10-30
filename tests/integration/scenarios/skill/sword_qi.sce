# sword_qi.sce — 剑气三连：绝招施放走 Lua→C++ 攻击入口
#
# 覆盖（§6 拍板 B 重开条款兑现，升回确定性断言）：
#   l_combat_attack（Combatd.cpp:86）——sword_adv.lua do_qi 内三次
#     combat_attack(me, target, TYPE_QUICK)；供给源 = L9 剑气供能
#     （set_skill sword_adv 越过 60 门槛 + mp 150 越过 100 门槛）。
#   ⚠ 收益口径修正：combat_attack_unarmed / combat_attack_weapon 教材
#     零调用方（全库 grep 证实，do_qi 只走通用 combat_attack），那两条
#     继续留认领清单，本段不承诺。
#
# 对手：半山腰铁人桩（sample_iron_dummy.lua，2000 血耐打、素人战力）——
#   绝招的教材喂招对象，三连击打不死它，与木人（prepare_combine 的短平快
#   对手）分工明确。
#
# 判定：
#   施放 → 本人版「你运足内力，将真气灌注剑身，连刺三道凌厉的剑气！」
#     （message_vision $N 渲染为「你」；半山腰同房别道看到的是名字版）
#   三连击真实执行 → l_combat_attack 覆盖即达成；本段**非战斗施放**
#     （check_ap 对非战斗直接放行——L3 偷袭的 /cast throwing sneak 同款
#     先例），不进比试房、无 AP 攒取与血量收场的时序竞态。
#
# 前置（本道既有）：learn_sword 学会 sword_adv 并已持铁剑（/cast sword 的
#   引擎入口查「使用的武器不对」——持剑是前置）、enable_practice 已
#   /enable sword sword_adv（「请先准备技能」前置）。
# 目标纪律（§8.5）：/t tie ren zhuang 定向铁人桩，段尾 /t 0 收干净。

cmd /t 1
cmd /tell 剑气供能请求，剑气供能账号是 $acct
wait text 剑气供能完毕 timeout=300000
wait attr mp >= 100 timeout=10000

cmd /t 0
cmd /t tie ren zhuang
wait object 铁人桩 timeout=10000
clear
cmd /cast sword qi
wait text 连刺三道凌厉的剑气 timeout=10000
cmd /t 0

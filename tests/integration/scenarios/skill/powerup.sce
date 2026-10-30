# powerup.sce — 激励术：战斗前施放 → 引擎记账式临时增益 → 心跳到期自动回扣
#
# 覆盖（unhit 函数 3 条，§6 决策认领段重开）：
#   l_start_powerup（CharCombat.cpp:439，force_adv.lua do_powerup 内 self: 调用）
#   l_remove_powerup（CharCombat.cpp:424，do_powerup 收功分支内 me: 调用——
#     绑定收 CContainer，故在角色对象上解除而非技能对象）
#   CSkill::remove_powerup（Skill.cpp:222，两条路都收：收功分支主动调 + 末段
#     call_out 心跳到期自动触发——记账回扣 apply/attack|dodge + send_state
#     + 到期提示，幂等守卫在无 powerup 态直接返回）
# 教材：lua-sample/skill/force_adv.lua 的 do_powerup（激励术，CAST_IN_BOTH
#   非敌意自身增益）——duration=内力等级（心跳拍），attack/dodge=等级/3，
#   到期由 CSkill::start_powerup 内 call_out 驱动，提示经 tell_object 私发。
#
# 判定：
#   施放 → 本人版「你微一凝神，运起高级内功…」（message_vision $N 渲染为「你」）
#   收功 → 本人版「你将真气收回丹田，解除了激励。」（提前解除，silence=1
#     免发到期提示，收功与到期两条回执文案互不混淆）
#   再施放 → 再次进入激励态（守卫已由收功分支清态，二次 start 不撞
#     「已经在运功」的旧拒绝支）
#   到期 → 私发「你的高级内功运行完毕，将内力收回丹田。」——心跳拍驱动
#     （§8.1：等 start_busy/心跳计时只能用 wait 收敛，禁 sleep 赌时序）
#   duration = 内力等级（本道 force 10 级 ≈ 10 拍），timeout 30s 是宽兜底。
#
# 前置（本道既有）：learn_force 学会 force_adv、enable_practice 已
#   /enable force force_adv（该行顺带把 mp 清零——正是 L9 供能要设 mp 的原因）。
# 供能（§3.3 协议）：L9 /call set max_mp + mp——mp≥100 是 do_powerup 的硬门槛，
#   打坐攒 100 mp 远超 E2E 时长预算，巫师供能是 §6 既有供给源口径。
# 请求词/回执词全服唯一；私发 tell 双端仅本账号可见（§8.6①）。

cmd /t 1
cmd /tell 激励供能请求，激励供能账号是 $acct
wait text 激励供能完毕 timeout=300000
wait attr mp >= 100 timeout=10000

cmd /t 0
# do_powerup 校验 cast 目标必须是自己——/t 定向自己账号后再施放（§8.6⑥：
#   /t 只收 ID，玩家=数字账号 ID），段尾 /t 0 收干净
cmd /t $acct
clear
cmd /cast force powerup
wait text 你微一凝神，运起高级内功 timeout=10000

# 提前收功：第二次施展走收功分支（l_remove_powerup）。
#   法术成功后引擎挂 1 拍 no_cast 冷却（LuaSkill.cpp cast 成功支的 callout），
#   心跳拍驱动、无状态可 wait——§8.1 心跳计时的合法 sleep 用途，
#   故每次再施放前先等一拍
clear
sleep 2000
cmd /cast force powerup
wait text 你将真气收回丹田，解除了激励 timeout=10000

# 三次施放重新进入激励态，等心跳 callout 自动到期（CSkill::remove_powerup）
clear
sleep 2000
cmd /cast force powerup
wait text 你微一凝神，运起高级内功 timeout=10000
wait text 将内力收回丹田 timeout=30000
cmd /t 0

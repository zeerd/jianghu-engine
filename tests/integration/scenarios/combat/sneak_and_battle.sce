# sneak_and_battle.sce — 偷袭青蛇成战（N4 拆分后：技能与 mp 由 L9 供能）
#
# 覆盖：
#   lua-default/skill/basic/throwing 的 do_sneak：先找暗器（wield 的棋子）、
#     再看内力（mp≥100 才放行，否则回「你的法力不足」）、命中/落空两分支
#   CCmdStandard::enable 的 throwing 映射（throw_adv 承接——基本技能不能
#     自我映射，enable throwing 回「是基础，不需要 enable」）
#   战斗结算（wait battle / battle_end）
#
# N4 拆分（§3.3）：throwing/throw_adv 技能与 mp 由 L9 巫师供能
#   （lane_wizard.sce 偷袭供能段：set_skill throwing/throw_adv + call set mp 400）；
#   /enable 留本道自做（用户命令）。原「第二轮打坐攒 mp≥100」段随拆分取消——
#   exercise 链（exercise_max_mp/halt_exercise）迁 L10，本道不再打坐、不再
#   请教、不再耗气，战前无需补药。
#
# 覆盖目标（N2 甄别转入）：偷袭落空判定是 random(throwing)==0（单投约
# 1/throwing ≈10%），且每次偷袭扣 30 mp（落空也扣）——L9 供 mp 400 给四投
# 预算：全落空路径 400-30×4=280；首投命中转战斗路径每击扣 100，
# 400-30-100×3=70 末投前仍 ≥100 不撞「法力不足」门槛。receipt 后用 attr
# 断言真实状态（§8.4①）。三投全落空（约 0.1%）→「中暗器了」断言红：把
# kill_ob/l_kill_ob 覆盖丢失变成显式失败，而不是静默漂移。
#
# 判定：「准备偷袭」由 tell_object 私发；命中「中暗器了」、落空「躲过了你的
#   偷袭」也只发给本人/本房，而山顶只有本道。
#
# 时序：L9 登录后约 10 秒即进入 wait text 偷袭供能请求（宽 timeout），
#   本道约百秒后到达服务点 → 请求方发完即进 wait（§3.3），回执毫秒级。

# --- 请求 L9 供能（throwing 技能 + mp 400）-----------------------------------
cmd /t 1
cmd /tell 偷袭供能请求，偷袭供能账号是 $acct
wait text 偷袭供能完毕 timeout=300000
wait attr mp >= 400 timeout=10000

# --- 激发高级投掷（技能已由 L9 set_skill 供能；enable 是用户命令留自做）------
cmd /t 0
clear
cmd /enable throwing throw_adv
wait text Ok. timeout=10000

# ---------------------------------------------------------------------------
# 投掷实战：非战斗命中走「只结仇恨不掉血」分支——kill_ob 的唯一入口；
# 落空走「侧身躲过」分支；命中开战后重投落战斗分支（「暗器伤害」，扣 100）。
# 固定三投（DSL 无分支）：任一非战斗命中即结仇开战；wait 匹配的是累积缓冲
# （本段无 clear），首投后 wait 即被旧文本满足——真正兜时序的是投间 sleep
# （busy 上限 2 拍），结果收敛由末尾断言把关。三投全落空（约 0.1%）→
# 「中暗器了」断言红：把覆盖丢失变成显式失败，而不是静默漂移；战斗本身
# 仍由统一 kill 收口（已在战斗中时 kill 是冗余重申，未开战时它补上这一战）。
# ---------------------------------------------------------------------------
wait object 青蛇
cmd /t qing she
clear
repeat 3
cmd /cast throwing sneak
wait text 中暗器了|躲过了你的偷袭|暗器伤害 timeout=15000
sleep 3200
end
wait text 中暗器了 timeout=10000
sleep 3000
cmd /kill
wait battle
wait battle_end timeout=180000
cmd .info
cmd /t 0

# --- 战利品与战后口粮（掉落 → 拾取 / 食物回血）----------------------------
# 青蛇的 die 把蛇胆丢在脚下（见 npc/sample_qingshe.lua），battle_end 时已在地上。
# 先 sleep 等战斗 busy 过拍——busy 由心跳驱动，是 sleep 的合法用途（§8.1）；
# 捡东西与吃东西都要求不忙
sleep 5000
wait object 蛇胆 timeout=10000
cmd /t she dan
cmd /get
wait item 蛇胆 timeout=10000
cmd /t 0

# 包子吃在战后残血窗口：food_base 只在真回血时才拼出 hp+，满血吃不出这串
cmd /t bao zi
cmd /use
wait text 你将剩下的包子吃得干干净净。 hp+ timeout=10000
cmd /t 0

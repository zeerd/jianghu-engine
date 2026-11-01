# raid_prepare.sce — L8（山贼试炼道）的战前准备：冲级 → 顶满气血 → 拜师 → L9 供能 → enforce
#
# 覆盖目标（2026-11-01 破障实测修订）：原「`l_receive_wound` 反噬分支」目标
# **已判结构性不可达、列入不测**（force.lua:39-41 的 mp_you ≤ 3mp 封顶使
# damage<0 恒不成立——⌊3mp/100⌋ ≤ ⌊mp/30⌋ 恒且 factor ≥ 1 → damage ≥ 1；
# 旧分析「山贼 mp500 → 0+1-5=-4<0」漏看了封顶）。
# 现行覆盖对象：Attack.cpp:364 force hit_ob 三闸的正常 ≥0 分支（mp_factor、
# mp≥factor、force 激发——本段供能链保证三闸全开）、enable 换功清 mp 分支、
# enforce 命令本身。
#
# 各段依据（attrs=10 14 30 30：con30 是本道的生存轴）：
#   · 升级 ×9（1→10 级）：pk_prep 同款——room_look_inquiry 的 5 万 combat_exp 撑满
#     level2_exp；con30 下 max_hp = 300+10×30×3+60(age16) = 1260。
#     **/upgrade 保留本道自做**：max_hp 1260 的重算是战斗窗口与药量核算的地基，
#     `call set level` 直写 dbase 不触发 setup_char 重算（与 L5 不同，本道
#     max_hp 绝对值进战斗窗口设计，不可替代）。
#   · 战前满状态：金创 9 粒把 eff 360→1260（金创只推 eff，首轮实测不推 hp），
#     回春 9 粒把 hp 360→1260——**hp 满量是战斗窗口的地基**：
#     · 战斗窗口（裸 hp，战斗中不吃药——/use 需 50 AP 实测拒）：双贼实测节奏
#       ~40 点/秒 → 1260/40 ≈ **31 秒**撑到败退——我方命中与山贼出招（chat
#       拍数被动触发）都在这个窗口内发生，**本道不以打赢为目标**
#       （见 raid_assault 头注）。
#   · force 链与三段打坐（N4 起）：L9 巫师供能（set_skill force/force_adv +
#     call set max_mp 4/mp 4），见下方服务段注——覆盖家分别是 L2 learn_force、
#     L3 force_for_exercise 与 L3 exercise_max_mp/halt_exercise；
#     force≥10 同时保住 enforce 的 factor 上限 force/2 ≥ 1。
#   · `/enforce 1`：mp_factor=1 —— factor 上限 force/2；取 1（不取大）是历史
#     反噬设计的延续（2026-11-01 后反噬已判不可达，但 1 仍是「运内力伤敌」的
#     最小正整数、足以开 Attack.cpp:364 的 mp_factor 闸）。enforce 会 send_state，
#     进战斗前设好、与战斗中的表演无关。
#
# 时长预算：本段约 80s（升级 35 + 战前药 58 − 请教/打坐 55 + 服务/动线 17+，
# 服务 receipt 通常毫秒级）。
#
# 判定纪律：升级/吃药/Ok. 全为私发或本人版；供能后用状态断言（`wait attr mp >= 4`）
# 而非文案（§8.4①）；供能的请求/回执都是私发 tell（L8↔L9 一对一）。

# --- 冲级：1 → 10 级（历练已由 common/room_look_inquiry 领取，con30 → max 1260） -
repeat 9
clear
cmd /upgrade
wait text 升级成功 timeout=10000
end

# --- 战前满状态：金创顶 eff 360→1260（9 粒），回春补 hp 360→1260（9 粒） ------
repeat 9
cmd /t jin chuang yao
clear
cmd /use
wait text 你吃下一包金创药|你现在不需要用金创药 timeout=10000
sleep 3200
end

repeat 9
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 0

# --- 拜师（common/apprentice 已把本账号收为青云子弟子，半山腰）--------------
# force 链与三段打坐改由 L9 巫师供能（N4 §3.3）：
#   · force 链（10×learn force + force_adv）→ set_skill force 10 + force_adv 10
#     ——覆盖家：L2 learn_force、L3 force_for_exercise（请教链与 valid_learn
#     门槛各自在案）；force≥10 同时保住 enforce 的 force/2 ≥ 1 上限；
#   · 三段打坐（max_mp 0→2 + halt 保 mp 4）→ call set max_mp 4 + call set mp 4
#     ——覆盖家：L3 exercise_max_mp / halt_exercise（收工抬升与 halt 压制同源）；
#     mp=4 满足 mp≥factor 的三闸条件（破障实测后反噬分支已列不测，
#     mp 供能保留理由见头注——三闸正常分支 + 服务链消费方）。
#   · 顺序硬约束：enable force 会把 mp 清零（CmdStandardMaster 661）——
#     技能/max_mp 供能在 enable **前**，mp 供能在 enable **后**，故拆两次请求。
#   · /enable 与 /enforce 留本道自做（用户命令：enable 的「内力必须重新修炼」
#     分支与 enforce 的 mp_factor=1 设置都是覆盖对象）。
# 请求/回执私发（§8.6①），请求词与 capture 标记全服唯一；发完即进 wait。
cmd /t 1
cmd /tell 山贼技能供能请求，技能供能账号是 $acct
wait text 山贼技能供能完毕 timeout=300000

# 激发：把 force 槽映射到 force_adv（enforce 的 query_skill_mapped 前置）
# 「内力必须重新修炼」= enable 的 force 换功分支（顺带把 mp 清零，故 mp 供能放它之后）
cmd /t 0
clear
cmd /enable force force_adv
wait text 内力必须重新修炼 timeout=10000

# --- 内力供能：enable 清零后由 L9 补 mp 4（force hit_ob 的 mp≥factor 闸）---------
cmd /t 1
cmd /tell 山贼内力供能请求，内力供能账号是 $acct
wait text 山贼内力供能完毕 timeout=300000
wait attr mp >= 4 timeout=10000

# --- enforce：mp_factor=1（force hit_ob 的 mp_factor 闸；必须在 enable 之后） ---
cmd /t 0
clear
cmd /enforce 1
wait text Ok. timeout=10000

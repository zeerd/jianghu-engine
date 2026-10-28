# lane_combat.sce — L3 道：山顶战斗尾
#
# 职责：force 线练到高级并激发 → 打坐把**内力上限**从 0 抬到 1（一次收工只抬
#   1 点）→ 投掷线练到高级并激发 → 上山顶取经验果（上限一次性 +100 到 101）→
#   第二轮打坐把**当前内力**从 1 攒到 ≥100（上限不动，累加不被 2×上限 压回）→
#   偷袭山鸡成战 → 等尸体腐烂 → 闯入山寨并离开副本。
#
# 为什么这一整段必须归同一条道：
#   · 偷袭要当前 mp≥100，而当前内力只能靠「经验果抬上限 → 打坐逐拍换气攒进
#     去」，树箱却全服只刷一只、经验果只有一粒（random/tree 置位后不再重排）；
#   · 尸体腐烂播报没有「你」字、只能靠房间隔离，故本道独占山顶；
#   · 山鸡只在房间 reset（900 秒）时补充，一次运行基本只有一次击杀机会；
#   · 打坐要求 force 已 enable，cast 偷袭要求 throwing 已 enable，两条技能线
#     因此都是本道的前置。
#
# 本道独享山顶与树箱，其余各道不进入山顶，也就不会与腐烂播报串扰。

include common/login.sce
include common/room_look_inquiry.sce
include provision/combat_supplies.sce
include common/apprentice.sce
include combat/levelup_heal.sce
include combat/force_for_exercise.sce
include combat/exercise_max_mp.sce
include combat/halt_exercise.sce
include combat/learn_throwing.sce
include combat/treebox.sce
include combat/sneak_and_battle.sce
include combat/corpse_and_dungeon.sce

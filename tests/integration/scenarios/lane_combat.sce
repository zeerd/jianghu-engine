# lane_combat.sce — L3 道：山顶战斗尾（N4 拆分后）
#
# 职责：上山顶取经验果 → 偷袭青蛇成战 → 等尸体腐烂 → 闯入山寨并离开副本。
#
# N4 拆分（§3.3）：原「练功/学习任务」五段（levelup_heal、force_for_exercise、
#   exercise_max_mp、halt_exercise、learn_throwing）已迁往 L10 练功道；本道
#   不再升级、不再请教、不再打坐——throwing 技能与 mp 由 L9 巫师供能
#   （lane_wizard.sce 偷袭供能段），`/enable throwing throw_adv` 留本道自做
#   （用户命令）。拆分后本道约 240s，非关键路径（当前关键路径 L8/L3 双峰
#   收敛为 L8 单峰 ~350s）。
#
# 为什么这一整段必须归同一条道（拆分后逐条重核，§3.3 末条）：
#   · ~~偷袭要当前 mp≥100，而当前内力只能靠……经验果只有一粒~~——**已解除**：
#     mp 由 L9 供 400（四投预算），经验果的 +100 上限不再是偷袭的前置；
#     但树箱经验果本身（random 刷箱 / Add_Box / do_use）是世界覆盖，钉在本道；
#   · 尸体腐烂播报没有「你」字、只能靠房间隔离，故本道独占山顶——**仍成立**；
#   · 战利品（青蛇掉的蛇胆）与战后口粮（包子）随本道的山顶流程走——掉落与
#     残血窗口都只有本道碰得到；
#   · 青蛇只在房间 reset（900 秒）时补充，一次运行基本只有一次击杀机会——
#     **仍成立**；
#   · ~~打坐要求 force 已 enable，cast 偷袭要求 throwing 已 enable，两条技能线
#     因此都是本道的前置~~——**已解除**：打坐迁走、throwing 改巫师供能，
#     本道只剩 enable 这一条用户命令。
#
# 本道独享山顶与树箱，其余各道不进入山顶，也就不会与腐烂播报串扰。

include common/login.sce
include common/room_look_inquiry.sce
include provision/combat_supplies.sce
include combat/treebox.sce
include combat/sneak_and_battle.sce
include combat/corpse_and_dungeon.sce

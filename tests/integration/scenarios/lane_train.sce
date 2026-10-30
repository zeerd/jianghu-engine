# lane_train.sce — L10 道：练功（N4 L3 拆分产物）
#
# 职责：承接原 L3 山顶战斗尾的五段练功/学习任务（§3.3 拆分条目）——
#   upgrade 门槛链、force 链、两轮收工打坐、halt 保 mp、throwing 链。
#   L2 已 288s、+130s 会把预算余量吃穿（§3.3「不足则评估新练功道」），
#   故开本道；时长约 180s，非关键路径（当前关键路径 L3 约 250s+）。
#
# 为什么这五段必须同道（迁移后仍成立的耦合）：
#   · levelup_heal 升到 3 级是后续请教的硬门槛（level2skill 对 5 级以下
#     给 (lv+1)*4，1 级角色技能上限只有 8，教不进 force/throwing）；
#   · force 链（force_for_exercise 末尾 enable force force_adv）是两轮
#     收工打坐的准入前置（exercise 拒未 enable 内功者），exercise_max_mp
#     与 halt_exercise 依次叠在这条 enable 之上；
#   · throwing 链（learn_throwing）与 force 链共用 levelup_heal 的 3 级上限
#     与补气循环，且 throw_adv 的 valid_learn 要求「基本投掷 ≥10 且手持
#     暗器」——棋子由本道自购（provision/train_supplies）。
#
# 稀缺资源归属（§8.4③）：本道自购回春丹 30 粒 / 金创药 2 包 / 棋子 1 盒
#   （商人共享库存：回春 200 / 金创 200 / 棋子 20，各道合计不超）；
#   练功木人（半山腰）归 L2，本道不触碰；不进任何副本、不发任何跨道信号。
#
# 判定纪律：与原 L3 各段一致——请教/吃药/升级全为私发或本人版回执，
#   每次迭代先 clear；关键状态用 attr 断言（max_hp 531 / eff_hp 缺口与补满 /
#   max_mp 抬升 / mp≥2 保底）。

include common/login.sce
include common/room_look_inquiry.sce
include provision/train_supplies.sce
include common/apprentice.sce
# 段序即依赖：upgrade 门槛（3 级）→ force 链（enable）→ 两轮收工打坐 →
# halt 保 mp → 巫师供大上限 + 收工不抬升补全 → throwing 链
# （throw_adv 需手持棋子，本道自购）
include combat/levelup_heal.sce
include combat/force_for_exercise.sce
include combat/exercise_max_mp.sce
include combat/halt_exercise.sce
include combat/exercise_no_raise.sce
include combat/learn_throwing.sce

# lane_skill.sce — L2 道：武学链
#
# 职责：拜师学艺 → 基础技能与两条高级技能的 valid_learn 门槛 → 四条命令错误
#   分支 → 练功木人对练（type=mirror 镜像NPC）→ 升级 → 睡觉 → force/sword
#   两条技能线各练到高级并激发 → enable 列表与 practice/cast 的错误分支。
#
# 为什么这条线独占睡觉与高级武功学习：
#   · 睡觉的醒来播报有房间版，本道与 L4 都会睡，但双方都只认「你一觉醒来」
#     本人版，故可同房并行；
#   · 学艺补气是本道自己的回春丹（provision/study_supplies.sce 采购），不与别道争抢；
#   · 升级/学会高级武功是本道的历练去处，L3 走的是另一条 force+throwing 线；
#   · 练功木人被 /ask 对练后会抄成问话人的样子（共享 NPC 的状态），只有本道
#     与它对话，别道既不问它也不打它，故对练结果不会互相踩。

include common/login.sce
include common/room_look_inquiry.sce
include provision/study_supplies.sce
include skill/learn_basic.sce
include skill/mirror_dummy.sce
include skill/levelup_rest.sce
include skill/learn_force.sce
include skill/learn_sword.sce
include skill/enforce_disabled.sce
include skill/enable_practice.sce
# 剑气三连（§6 拍板 B 重开条款：adv≥60 + 战斗中 mp≥100 由巫师供能）：
#   必须在 enable_practice 之后（sword 已 enable，cast 前置），
#   在 prepare_combine 之前（后者卸铁剑——/cast sword 查「使用的武器不对」），
#   木人先切磋（threshold 不死）后由 prepare_combine 击散，顺序不可倒。
include skill/sword_qi.sce
# 空手组合（一次两招）：请求 L9 供能 → 卸剑 → enable 双空手槽 → prepare
# 成功分支 → 打木人断言双击。必须在 enable_practice 之后（其 prepare 拒绝
# 分支先建立「pmap 为空」的前提与反面断言），在 skill_admin 之前（abandon
# 收口在道尾，其后不接任何交互）。
include skill/prepare_combine.sce
# 激励术（一放一收 + 心跳到期）：请求 L9 供能 mp≥100 后连施三次。
#   必须在 enable_practice 之后（force_adv 已 enable——cast 前置），
#   在 skill_admin 之前（abandon 收口在道尾，其后不接任何交互）。
include skill/powerup.sce
include skill/skill_admin.sce

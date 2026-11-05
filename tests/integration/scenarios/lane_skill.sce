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
# 剑气三连（adv≥60 + mp≥100 由巫师供能）：
#   必须在 enable_practice 之后（sword 已 enable，cast 前置），
#   在 prepare_combine 之前（后者卸铁剑——/cast sword 查「使用的武器不对」），
#   木人先切磋（threshold 不死）后由 prepare_combine 击散，顺序不可倒。
#   本段的空手半边会 /enable hand hand_adv + /prepare hand_adv，段尾
#   /prepare none 把 pmap 还成空——prepare_combine 的两技能 /prepare 只在
#   pmap 为空时走成功分支，借完即还是它的前置。
include skill/sword_qi.sce
# 空手组合（一次两招）：请求 L9 供能 → 卸剑 → enable 双空手槽 → prepare
# 成功分支 → 打木人断言双击。必须在 enable_practice 之后（其 prepare 拒绝
# 分支先建立「pmap 为空」的前提与反面断言），在 skill_admin 之前（abandon
# 收口在道尾，其后不接任何交互）。打散的练功木人随 die 钩子自毁，它 /kill
# 时写进杀意名单的账也一并不留——本段之后道上没有活名单。
include skill/prepare_combine.sce
# 激励术（一放一收 + 心跳到期）：请求 L9 供能 mp≥100 后连施三次。
#   必须在 enable_practice 之后（force_adv 已 enable——cast 前置），
#   在 skill_admin 之前（abandon 收口在道尾，其后不接任何交互）。
#   必须在 iron_dummy_retaliate 之前：/kill 写下的名单在玩家每次进房时都由
#   CChar::init 的 hatred 分支重排一颗 5~10 秒后引爆的自动开战，而本段两处
#   sleep 正落在那个窗口里——战斗中 /cast 会撞上「需要…AP，你的AP不足」。
include skill/powerup.sce
# 铁人桩的回合还手（attack 钩子 → base_attack，钩子在其己方回合派发、一回合一
#   击，非受击额外攻击）：开战、第二次还手后 /halt 逃战收场，本段不换装备不动技能。
#   排在 skill_admin 之前是刻意的：铁人桩打不死，它是本道最后一处 /kill、名单
#   一直有效——本段之后只剩 skill_admin 约一秒，雷落下时道已收口。断言全取
#   铁人桩一侧的文本，与手上有没有剑无关。
include skill/iron_dummy_retaliate.sce
include skill/skill_admin.sce

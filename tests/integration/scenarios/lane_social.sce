# lane_social.sce — L4 道：社交生活
#
# 职责：账号类命令（前置段，含 DB 密集的改密链）→ NPC 师徒家门查询（family/user
#   回归）→ 睡觉恢复 → 观察师父（同门关系分支）→ 毒药与补药对照（涂毒挂
#   poison/modu + 状态生命周期）→ 传送符换场景 → 观察人物两条路 → 说话反应 →
#   钱庄四条分支 → 频道发言回显与新手频道回显 → 社交表情 → 频道禁词 → answer
#   频道接管（天下大考）→ 领历练升级与投票 → 师父验看（门规与实学核验）→
#   频道切换（tune 尾段）→ 判师改投黑风寨与「悔过」消记录（道尾）。
#
# 为什么这些归同一条道：
#   · 毒药对照要先把气血打残再按序补满，是同一位角色身上的一段连贯流程；
#   · 蛇胆的吞服只发生在本道（本道买来服下；L3 捡到的那颗原样留着），
#     其房间版播报不会被别的账号触发；
#   · 涂毒的三前置（毒术供能、自持的铁剑、内力）都接在这条链上，拆道即断；
#   · 本道不学习技能；历练走正常入账（大考答对 + 拜师入门礼），供投票的
#     等级门槛（升 5 级）与师父验看的报数消费；
#   · 睡觉与半山腰的学艺会同房并行，但双方都只认「你…」本人版断言。

include common/login.sce
include common/attr_register_verify.sce
include provision/social_supplies.sce
include social/account.sce
include common/apprentice.sce
# 紧接拜师发家门查询请求（链首发请求，L9 尽早消费；本道随后才进长睡）
include social/npc_master_family_user.sce
include social/sleep_and_observe.sce
include social/poison_and_cure.sce
include social/look_and_emote.sce
include social/bank.sce
include social/channel_and_banword.sce
include social/vote.sce
# 师父验看（attr(str/con/int/per/dex) + attr(combat_exp)/query_valid_exp）：
#   必须接 vote 之后——历练与等级都是本道正常玩法攒出来的（大考 1000 +
#   拜师入门礼 50000 + 升 4 级），报数断言的是这组精确值；看完回山脚，道尾判师
#   以山脚起步（详见片段头注）。
include social/master_inspect.sce
include social/tune.sce
# 判师改投黑风寨 + 青云子「悔过」消判师记录（l_remove_betrayer）：必须在道尾——
#   悔过按判师次数降全部技能，其后不能再有吃技能/家门状态的片段（详见片段头注）。
include social/betray_and_repent.sce

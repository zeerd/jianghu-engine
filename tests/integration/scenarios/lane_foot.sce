# lane_foot.sce — L1 道：山脚基础 + 房屋
#
# 职责：房间里观察、向导问路与排行榜、开箱取物、商人买卖、任务、装备、茶棚，
#   再完整走一遍住宅区置产到卖房迁出。
#
# 为什么这些归到同一条道（并行隔离）：
#   · 开箱独占山脚木箱/药箱——全服唯一一份、取完即止，故取箱只在此道；
#   · 排行榜的应答是房间播报（say 无目标），只在此道触发并断言；
#   · 房屋从申请到卖房是同一角色的连贯流程，且本道自带现金与金创药。
#
# 其余各道各自登录、各自采购，互不依赖本道留下的任何东西。

include common/login.sce
include common/room_look_inquiry.sce
include mountain_foot/rank.sce
include mountain_foot/open_boxes.sce
include mountain_foot/buy_sell.sce
include mountain_foot/quest.sce
include mountain_foot/vendor_talk.sce
include mountain_foot/wield_sword.sce
include mountain_foot/tea_fill_drink.sce
include house/apply.sce
include house/servant.sce
include house/rename_and_sell.sce

# lane_foot.sce — L1 道：山脚基础 + 房屋
#
# 职责：房间里观察、向导问路与排行榜、开箱取物、商人买卖、任务、装备与卸装、
#   茶棚（装茶饮用后丢弃酒壶），再完整走一遍住宅区置产到卖房迁出，最后主动
#   退出（quit 收口连接，见 lane 尾注释）。
#
# 为什么这些归到同一条道（并行隔离）：
#   · 开箱独占山脚木箱/药箱——全服唯一一份、取完即止，故取箱只在此道；
#   · 锁着的宝箱与铜钥匙（password 配套）同归此道——开锁广播是房间播报，
#     全服只有一把钥匙、只有本道会开；
#   · 排行榜的应答是房间播报（say 无目标），只在此道触发并断言；
#   · 房屋从申请到卖房是同一角色的连贯流程，且本道自带现金与金创药。
#
# 其余各道各自登录、各自采购，互不依赖本道留下的任何东西。

include common/login.sce
include common/room_look_inquiry.sce
include mountain_foot/rank.sce
include mountain_foot/open_boxes.sce
include mountain_foot/buy_sell.sce
# 锁箱段须在 buy_sell 之后（向商人买钥匙），且钥匙/锁箱归本道独占（§8.4③）
include mountain_foot/locked_box.sce
include mountain_foot/quest.sce
include mountain_foot/vendor_talk.sce
include mountain_foot/wield_sword.sce
include mountain_foot/tea_fill_drink.sce
include mountain_foot/item_ops.sce
include house/apply.sce
# 房内存取物：须在置业入宅之后、仆役段之前（仍在宅院内），单道独有
include house/box_store.sce
include house/servant.sce
include house/rename_and_sell.sce
# 片尾三条命令流程自验（山脚向导在场 + 本道无伴侣/无收徒记录）——必须在 /quit 之前
include mountain_foot/recruit_teach_guard.sce

# 道收口：主动退出——整道最后一步，理由与挂道依据见片段头注
include common/quit.sce

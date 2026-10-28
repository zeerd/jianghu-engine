# lane_foot.sce — L1 道：山脚基础 + 房屋
#
# 职责：房间里观察、向导问路与排行榜、开箱取物、商人买卖、任务、装备与卸装、
#   茶棚（装水饮用后丢弃茶壶），再完整走一遍住宅区置产到卖房迁出，最后主动
#   退出（quit 收口连接，见 lane 尾注释）。
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
include mountain_foot/item_ops.sce
include house/apply.sce
include house/servant.sce
include house/rename_and_sell.sce

# /quit 必须是整道最后一步：quit 触发服务端 DESTRUCT 断连，
# 其后任何 wait 都会因 "server closed the connection" 判失败（剧本器
# 每步开始前先查 server_closed），故这里不再有后续片段。
# quit 的可观测结果即「连接正常收尾」：报告 pass + CLI 退出码 0 收口；
# 前置条件由 wait battle_end 兜底（quit 的两个拒绝分支是战斗中/忙碌，
# 本道全程无战斗、片尾无 busy）。
#
# ⚠ /quit 挂在本道（最速道，约 25~30s 收尾）而不是 L4——quit 触发的
#   save_to_server 是全量落库，会把单线程工作线程停顿数秒；L4 若在道尾
#   quit，正好撞上 L3 的 heal→busy 命令边界（run2 红的根因）。L1 收尾时
#   L2 的首个 heal 边界约 48s、L3 约 150s 都还没到，全道无任何 busy 边界。
wait battle_end
cmd /quit

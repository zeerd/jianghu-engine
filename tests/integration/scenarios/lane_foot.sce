# lane_foot.sce — L1 道：山脚基础 + 房屋
#
# 职责：房间里观察、向导问路与排行榜、开箱取物、商人买卖与盲盒抽奖、任务、
#   装备与卸装、茶棚（装茶饮用后丢弃酒壶），再完整走一遍住宅区置产到卖房
#   迁出，最后主动退出（quit 收口连接，见 lane 尾注释）。
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
# 卸力前置（供能 + 激发 + 脱战起势负支）：本道唯一的服务请求，越早发供能
#   请求，L9 那头越不会滞留后到的请求（服务链链首先消费它）；起势/持续/收势
#   三步长在 road_rogue 已开打的死战里（本道唯一归己的战斗对手是山道强人，
#   且起势必须在战斗中），分工见该段头注与 parry_xieli_prep 头注
include mountain_foot/parry_xieli_prep.sce
include mountain_foot/open_boxes.sce
include mountain_foot/buy_sell.sce
# 锁箱段须在 buy_sell 之后（向商人买钥匙），且钥匙/锁箱归本道独占（§8.4③）。
# 必须排在盲盒**之前**：盲盒奖池 13 件里有铜钥匙，抽中时本道已持钥，
# 「空手 open → 你无法打开」这一支就永远走不到（实测抽中即红，约 1/13）
include mountain_foot/locked_box.sce
# 盲盒现买现拆（xrandom 抽奖）：同在山脚、盘缠刚开箱到手，奖品只进背包、
# 后续没有任何片段依赖「身上没有它」
include mountain_foot/blind_box.sce
include mountain_foot/quest.sce
include mountain_foot/vendor_talk.sce
include mountain_foot/wield_sword.sce
# 林间空地的战斗状态链（记仇/暗器/销账/死亡）：起终点都在山脚。必须排在
# drink_and_faint **之前**——供能给的内力要留到本段开战用（醉昏会把 mp 清零
# 且苏醒不回补，卸力起势的 mp≥40 门槛会撞空），也排在 tea_fill_drink 之前
include mountain_foot/road_rogue.sce
# 饮烈酒昏过去再醒：全程在山脚不挪窝（昏过去期间不能动），上一段收尾在山脚，
# 且必须排在 tea_fill_drink 之前——后者第一句就是 /go 山脚茶棚，要求本道此刻在山脚
include mountain_foot/drink_and_faint.sce
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

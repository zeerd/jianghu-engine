# apply.sce — 住宅区：进入观察 → 申请成为居民 → 选房型/起名/描述/缴银登记
# → 进入宅院 → 宅院内部观察
#
# 覆盖：room/sample_resident.lua + lua-default/room/resident_base.lua
#   do_look 住户统计与申请入口 / applyhouse / applyroom / housename /
#   housedesc / registerhouse（缴银登记）/ update_register / set_house
#
# 前置：本道在前面已备好现金（起步本金 + 山脚木箱金币）；本区地价低廉，
#   单人间折半仅需 2500 金。
#
# 说明：登记会触发住宅区首次落盘的锁等待（约 4 秒），故登记段用显式超时。

cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚

cmd /go 青云城住宅区
wait room 青云城住宅区
clear
cmd /look
wait text 沿街是一排青砖小院 timeout=10000
wait text 住宅区。目前这里 timeout=10000
wait menu
menu 申请成为青云居民

# ---------------------------------------------------------------------------
# 申请房屋：选房型 → 起名 → 写描述 → 缴银登记
# （applyhouse → applyroom → housename → housedesc → registerhouse）
# ---------------------------------------------------------------------------
wait menu
menu 申请单人间
wait menu
menu 设置房间名称
menuinput 雅室
wait menu
menu 设置房间描述
menuinput 竹影横窗
wait menu
menu 申请房间
wait menu timeout=30000
menu 进入雅室
wait room 雅室 timeout=30000

# ---------------------------------------------------------------------------
# 宅院内部：房屋描述、房间描述、柜子与仆役
# ---------------------------------------------------------------------------
clear
cmd /look
wait text 私人房间 timeout=10000
wait text 竹影横窗 timeout=10000
wait object 柜子
wait object 仆人

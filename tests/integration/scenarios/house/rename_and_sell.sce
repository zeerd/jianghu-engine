# rename_and_sell.sce — 房屋改名/改描述 → 仆役拦人 → 拒客 → 名单与重入 → 卖房迁出
# → 重新入宅验证欢迎主人词 → 卖房迁出
#
# 覆盖：room/house_base.lua
#   housename → update_register（登记同步改名）/ housedesc /
#   handle_action 拦截 cast（仆役拦人）/ do_kick 的房主分支（拒客）/
#   住户名单（checkhouse）/ 回家（backhome）/ sellhouse → remove_house
#
# 说明：`/cast force roar` 由房屋 handle_action 在任何技能检查之前拦截
#   （house_base.lua:432），故本道无需任何武功即可覆盖该分支。

# ---------------------------------------------------------------------------
# 房屋改名（house_base.lua housename → update_register → 登记同步改名）
# ---------------------------------------------------------------------------
clear
cmd /look
wait menu
menu 修改房间名称
menuinput 静思斋
wait text OK. timeout=10000

# ---------------------------------------------------------------------------
# 房屋改描述（house_base.lua housedesc：成功时无回显，用再次观察验证落库）
# ---------------------------------------------------------------------------
clear
cmd /look
wait menu
menu 修改房间描述
menuinput 竹影横窗两宜
sleep 800
clear
cmd /look
wait text 竹影横窗两宜 timeout=10000

# ---------------------------------------------------------------------------
# 仆役拦人：宅院里的「抢书」被仆役挡下（house_base.lua handle_action）
# ---------------------------------------------------------------------------
clear
cmd /t 0
cmd /cast force roar
wait text 想抢书 timeout=10000

# ---------------------------------------------------------------------------
# 拒客：房主自己被「赶人」时引擎拒绝（house_base.lua do_kick 的房主分支）
# ---------------------------------------------------------------------------
clear
cmd /look
wait menu
menu 拒绝客人来访
menuinput $acct
wait text 人家可是房主 timeout=10000

# ---------------------------------------------------------------------------
# 离开宅院回到住宅区：房主视角（住户名单 / 卖掉）
# ---------------------------------------------------------------------------
cmd /go 出口
wait room 青云城住宅区
clear
cmd /look
wait text 卖掉静思斋 timeout=10000
wait menu
menu 查看有哪些住户
wait text 静思斋( timeout=10000

# ---------------------------------------------------------------------------
# 重新入宅（回家 → enter）：init → do_greeting1 播报 do_welcome2 设置的
# 欢迎主人词，验证欢迎词真实落库到房屋登记
# ---------------------------------------------------------------------------
clear
cmd /look
wait menu
menu 回家
wait room 静思斋 timeout=10000
wait text 恭迎主人回府 timeout=15000
cmd /go 出口
wait room 青云城住宅区

# ---------------------------------------------------------------------------
# 卖房迁出（sellhouse → remove_house）：估价 → 决定卖了 → 注销住户、销毁房屋
# ---------------------------------------------------------------------------
clear
cmd /look
wait menu
menu 卖掉静思斋
wait text 目前价值 timeout=10000
wait menu
menu 决定卖了
wait text 你卖掉了 timeout=10000
sleep 600
clear
cmd /look
wait text 住宅区。目前这里 timeout=10000
wait menu
menu 申请成为青云居民
cmd /t 0

# servant.sce — 仆役（npc/monster_yongren.lua）全套：
#   talk 认主问候（do_talk 靠 get_my_house 认主）/ 改名 / 改描述 /
#   欢迎客人词 / 欢迎主人词 / 出租物品定价（do_rent → do_value → do_price）/
#   代管取回（do_getobj → do_giveme 的主人分支）/ 拒收非租借品（accept_object）/
#   空帐户提款（do_withdraw）/ 不理会打斗（handle_action 拦截 kill/fight/cast）
#
# 说明：出租物品用金创药——本道自带（见 mountain_foot/buy_sell.sce），不依赖其他道。

clear
cmd /t 仆人
cmd /talk
wait text 主人回来了 timeout=10000

# 仆役改名（do_name：名字落库到房屋登记）；观察对象时目标仍是仆役，
# 其人物描述行带新名字——客户端的对象名录在改名后不刷新，只能走定向观察
menu 改名
menuinput 阿福
wait text OK. timeout=10000
sleep 400
clear
cmd /look
wait text 阿福 timeout=10000

# 仆役改描述（do_desc：成功时无回显，用定向观察验证落库）
cmd /talk
wait menu
menu 改描述
menuinput 年轻的女仆
sleep 400
clear
cmd /look
wait text 年轻的女仆 timeout=10000

# 仆役欢迎词（do_welcome1 访客词；do_welcome2 主人词稍后重新入宅验证）
cmd /talk
wait menu
menu 设置欢迎客人词
menuinput 贵客临门
sleep 400
cmd /talk
wait menu
menu 设置欢迎主人词
menuinput 恭迎主人回府
sleep 400

# 仆役出租物品（do_rent → do_value → do_price：定价后物品交仆役代管）
cmd /talk
wait menu
menu 出租物品
wait text 请选择租借或售卖的物品 timeout=10000
wait menu
menu 金创药
wait text 请定租借价格 timeout=10000
wait menu
menu 定价
menuinput 200
wait text OK，设定物品价格200 timeout=10000

# 仆役代管取回（do_getobj → do_giveme 的主人分支：主人无偿取回）
cmd /talk
wait menu
menu 租借物品
wait menu
menu 金创药 : 200
wait text 你改变主意，不卖金创药了 timeout=10000
wait item 金创药 timeout=10000

# 仆役拒收非租借品（monster_yongren.lua accept_object：只收带 yongren/rent
# 标记的物品。租借品的「收还」分支要客人还品，属多客户端能力，由单测覆盖。
# 断言用金创药：按名定位到的必然是玩家身上的药——全部未离身，无歧义）
cmd /give 金创药
wait text 对方不要你的东西 timeout=10000

# 仆役提款（do_withdraw：从未有代售利润的空帐户分支）
cmd /talk
wait menu
menu 提款
wait text 您并没有售出物品 timeout=10000

# 仆役不理会打斗（handle_action 拦截 kill/fight/cast）
clear
cmd /kill
wait text 皱了皱眉 timeout=10000
cmd /t 0

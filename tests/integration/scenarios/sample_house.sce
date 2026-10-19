# sample_house.sce — lua-sample 房屋/居民剧本（无头 E2E）
#
# 前置（register.sce + sample_mainline.sce + sample_social.sce 依次跑完，同一角色）：
#   角色在青云山山脚，身上有现金（山脚木箱里的金币，足够置产）。
#   注：住房门槛由住宅区自定（room/sample_resident.lua 的 policy 子映射）：
#   本区地价 5000 金、十四岁即可置产——示例世界的规则，与引擎缺省无关。
#
# 覆盖的引擎能力：
#   住宅区（room/resident_base.lua）：do_look 住户统计 + 申请成为居民 →
#     选房型 → 起名 → 写描述 → 缴银登记
#     （registerhouse/update_register/set_house）
#   房屋（room/house_base.lua）：进入自家宅院、房间描述、柜子、仆役、
#     改名/改描述、仆役拦人（cast force roar）、拒客（kickout 的房主分支）
#   仆役（npc/monster_yongren.lua）：talk 认主问候（do_talk 靠 get_my_house
#     认主）、改名/改描述、欢迎客人词/欢迎主人词（重新入宅由 init →
#     do_greeting1 播报）、出租物品定价（do_rent → do_value → do_price）、
#     代管取回（do_getobj → do_giveme 的主人分支）、拒收非租借品
#     （accept_object）、空帐户提款（do_withdraw）、不理会打斗
#     （handle_action 拦截 kill/fight/cast）
#   卖房迁出（sellhouse → remove_house）：估价、成交、注销住户、销毁房屋
#
# 确定性说明：
#   · 每一步都用「正向断言」等菜单/文案出现，不用 notext；
#   · 缴银登记会触发住宅区首次落盘的锁等待（约 4 秒），故登记段用显式超时；
#   · 出租的物品用金创药：sample_social.sce 收尾的「不需要用金创药」拒绝
#     分支保证角色身上至少留有一粒，菜单选择因此确定。

login $acct $pass
wait login
wait room 青云山山脚
sleep 800

# ---------------------------------------------------------------------------
# 住宅区：进入 + 观察（场景描述 + 住户统计 + 申请入口）
# ---------------------------------------------------------------------------
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
sleep 600
clear
cmd /look
wait text 私人房间 timeout=10000
wait text 竹影横窗 timeout=10000
wait object 柜子
wait object 仆人

# ---------------------------------------------------------------------------
# 仆役认主：talk 走 do_talk，靠 get_my_house 查访客在本城登记的住宅，
# 认主后有「主人回来了」问候并摊开房管菜单
# ---------------------------------------------------------------------------
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

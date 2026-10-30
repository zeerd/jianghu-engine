# trade_a.sce — 摆摊交易（A 方：领贸易文书 / 定价 / 收货款 / 下架）
#
# 与 trade_b.sce 配对并行。链路见 lua-default/item/trade.lua + src/cmd/CmdUserVendor.cpp：
#   1) 领文书：向山脚杂货老板索取（trade.lua 贸易文书按人发放，不占稀缺资源，
#     L1 的 vendor_talk.sce 只对 L1 账号发放，互不干扰）；
#   2) 定价：`/use 贸易文书`（自用分支）→ list_trade 菜单 `$2value $1 <obj> $2`
#     → `menu <商品>` + `menuinput <价>`；撤价同路径输 0——全程无需硬编码 obj_id；
#   3) 售卖：由 B 侧 listgoods+buy 完成（C++ CmdUserVendor::buy），A 断言
#     message_vision 本人版（$n 渲染为「你」）与货款金额；
#   4) 下架：茶壶走「定价超 B 现金 → 买不动」错误分支后撤价，
#     B 复核 listgoods 落空商品分支。
#
# 商品归属（§8.4③）：金创药/茶壶为本道自购待售品，不碰任何钉给唯一道的资源。

cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
# 换房后的对象推送晚于 wait room 一拍，先 wait object 再 /t 英文 id
wait object 杂货老板

# --- 领贸易文书（菜单发放，本人版递接播报 + item 到账） ----------------------
cmd /t huo ji
clear
cmd /ask 交易
wait menu timeout=10000
menu 索取一份交易文书
wait text 递给你一份交易文书 timeout=10000
wait item 贸易文书 timeout=10000

# --- 采购待售商品：金创药（正常成交） + 茶壶（高定价走金币不足分支） --------
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
cmd /buy 9
wait text 你从杂货老板那里买下了数量1的茶壶 timeout=10000
cmd /t 0

# --- 定价：金创药 100 --------------------------------------------------------
# use 命令需物品目标（§8.5）；定价回执是 tell_object 私发「完成定价！」，
#   随后文书重开菜单，展示行含新价格（断「金创药 ：100」证明价格已生效）
cmd /t trade
clear
cmd /use
wait menu timeout=10000
menu 金创药
menuinput 100
wait text 完成定价 timeout=10000
wait text 金创药 ：100 timeout=10000

# --- 定价：茶壶 2 亿（超 B 现金，供买方走金币不足分支） ----------------------
clear
cmd /use
wait menu timeout=10000
menu 茶壶
menuinput 200000000
wait text 完成定价 timeout=10000
wait text 茶壶 ：200000000 timeout=10000
cmd /t 0

# --- 通知 B 开买（单一同步信号覆盖两件商品的定价完成） ----------------------
cmd /t $peer_acct
cmd /tell 定价好了，来买吧
cmd /t 0

# B 买走金创药：A 侧 message_vision 本人版（$n→你），断定价金额证明货款入账
wait text 从你那里买下了金创药 timeout=30000
wait text 付给你数量100 timeout=10000

# B 买茶壶失败（金币不足）→ 回呼信号
wait text 买不起茶壶 timeout=30000

# --- 下架：撤茶壶定价（菜单路径 menuinput 0，tell 私发「取消出售。」） ------
cmd /t trade
clear
cmd /use
wait menu timeout=10000
menu 茶壶
menuinput 0
wait text 取消出售 timeout=10000
cmd /t 0

# 通知 B 复核（B 侧 listgoods 落空商品分支）
# ⚠ A 发完信号不能立刻离开山脚：B 的复核 listgoods 走 get_room_char，
#   要求目标（A）同房间。原写法靠 sleep 赌「B 会先复核完」，对端一慢就翻车；
#   改为等 B 的复核完成回执（双向握手），A 在此之前不离开。
cmd /t $peer_acct
cmd /tell 下架好了
cmd /t 0
wait text 复核好了 timeout=30000

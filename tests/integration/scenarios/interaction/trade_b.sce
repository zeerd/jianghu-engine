# trade_b.sce — 摆摊交易（B 方：listgoods 菜单 / 购买 / 错误分支 / 下架复核）
#
# 与 trade_a.sce 配对并行。B 对 A（摊主）执行：
#   listgoods（C++ CmdUserVendor::list_goods，菜单含 A 的定价商品）
#   → menu 选购（$0buy $1 <obj>，wire buy <A_id> <obj_id>）
#   → 错误分支：买不存在商品 / 金币不足（茶壶定价 2 亿 > B 现金 1000 万）
#   → A 下架后 listgoods 落空商品分支（「你对…的商品似乎不感兴趣。」私发）。
#
# 注：B 在 give 段已收到过 A 赠的一包金创药，背包 item 条件无法区分新购，
#   故本次成交以服务端成交文案（本人版「你从<peer>那里买下了金创药」+
#   货款金额）断言——文案只在 buy 成功路径渲染。

# 等 A 的定价完成信号（A 领文书+采购+两次定价约 15s，宽 timeout）。
# ⚠ 本段双方全程同房间（山脚）——/t 只收 ID（见 marriage_a 头注释），
#   同房间定向一律用对方数字账号 ID（$peer_acct）或对象英文 id；
#   B 先步行到山脚再等信号，A 的 tell 与 B 的回呼都在同房间内成立。
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000

wait text 定价好了，来买吧 timeout=40000
cmd /t $peer_acct

# --- 购买金创药（100 金）：菜单选购 + 成交文案含金额 -------------------------
clear
cmd /listgoods
wait menu timeout=10000
menu 金创药
wait text 你从$peer那里买下了金创药 timeout=10000
wait text 付给$peer数量100 timeout=10000

# --- 错误分支：买不存在的商品（目标在房、obj_id 不存在 → 「对方不打算卖给你」）
clear
cmd /buy 999999
wait text 对方不打算卖给你 timeout=10000

# --- 错误分支：金币不足（茶壶定价 2 亿，B 现金不够 → 摸摸口袋分支） ----------
clear
cmd /listgoods
wait menu timeout=10000
menu 茶壶
wait text 摸摸口袋 timeout=10000

# 回呼 A 下架（A 侧断言「买不起茶壶」）
cmd /tell 买不起茶壶
cmd /t 0

# --- 下架复核：A 已撤价 → listgoods 无商品 → 私发「不感兴趣」 ---------------
wait text 下架好了 timeout=30000
cmd /t $peer_acct
clear
cmd /listgoods
wait text 商品似乎不感兴趣 timeout=10000
# 回执 A「复核好了」——A 收到才离开山脚（listgoods 要求 A 同房间，见 trade_a 尾注）
cmd /tell 复核好了
cmd /t 0

# vendor_talk.sce — 商家：询问回退 + 索取交易文书
#
# 覆盖两段（lua-sample/npc/sample_vendor.lua）：
#   1. 询问回退：杂货老板是 CVendor（type="vendor"），问话不含「交易」时
#      脚本交回 base_do_talk → 未登记询问条目 → 直接回绝（CLuaNpcT::
#      base_do_talk → CNpc::do_talk_inquiry）。老李头这类非商人的 NPC 走的
#      是脚本侧 do_lua_inquiry，两条路互不重叠。
#   2. 索取交易文书：问话命中「交易」→ do_talk 列菜单 → get_trade_doc 发放
#      lua-default/item/trade.lua（贸易文书）；再索取走 notify_fail 私发拒绝（人手一份）。
#
# 并行说明：
#   · 播报断言取本人版「递给你一份交易文书」——message_vision 对 NPC（$N）
#     主语渲染旁人为「杂货老板递给<名字>…」，只有本人收得到「递给你」；
#   · 菜单是本人专属通道，别的道在杂货老板处只 /buy /sell，不会串扰；
#   · `/t 杂货老板` 是本道自己的目标，块尾 `/t 0` 收干净。
#
# 说明：回退分支只需本道触发一次；交易文书按人发放、不占稀缺资源。

cmd /t 杂货老板
clear
cmd /ask 商人
wait text 对方并不愿意理你 timeout=10000
sleep 600

# 索取交易文书：菜单发放，状态断言用 item（背包真实出现），文本断言取本人版
clear
cmd /ask 交易
wait menu timeout=10000
menu 索取一份交易文书
wait item 贸易文书 timeout=10000
wait text 递给你一份交易文书 timeout=10000
sleep 600

# 人手一份：再索取 → notify_fail 私发拒绝
clear
cmd /ask 交易
wait menu timeout=10000
menu 索取一份交易文书
wait text 你已经有一份交易文书 timeout=10000
sleep 600
cmd /t 0

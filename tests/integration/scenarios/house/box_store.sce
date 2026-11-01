# box_store.sce — 房主把随身药材存进自家柜子，再点名取回
#
# 业务：宅院内柜子的完整存取流程——存入（物品从背包进柜）→ 柜内点名取回
#   （look 内容菜单取物）→ 复看确认柜内清空。房主自己的财物进出，
#   全程发生在自家私宅，不经手任何他道资源。
#
# 判定与前置：
#   · 存入回执是 message_vision 本人版「你向柜子里丢下数量1的金创药。」，
#     旁人版渲染「<名字>…」；私宅内无他道账号，无从串满足；
#   · 取回走柜子 look 的内容菜单点名——菜单项不在即 fail-fast；随后复看
#     断「里面有：」在 3s 时间窗内不再出现（notext 全程监视到期满），
#     柜内真清空才算取回成立——背包本就有同名药，wait item 是弱断言，不用；
#   · 金创药 value=300，不走「并不值钱当场销毁」支；取回后与背包原有堆叠
#     合并，后续 servant.sce 租药、rename_and_sell.sce 卖房迁出不受影响。
# 前置：house/apply.sce 已置业入宅（本片由 lane_foot 在 apply 与 servant
#   之间 include，执行时人在宅院内）；背包金创药来自 buy_sell 购入与山脚
#   药箱（本道独占资源）。房内柜子随置业首建，sellhouse 迁出时随房注销。
# 目标纪律（§8.5）：/t box 只为定向观察，块末 /t 0 收干净。
# 放置：单道独有，不被别的道复用——宅院是 per-owner 实例，存取只发生
#   在本道自家柜子。

cmd /drop jin chuang yao into box
wait text 你向柜子里丢下数量1的金创药 timeout=10000

# 定向观察柜子：内容菜单出现（存入生效），点名取回
cmd /t box
clear
cmd /look
wait menu
menu 金创药

# 复看柜内：空柜不渲染「里面有：」，3s 时间窗内不出现才通过
clear
cmd /look
wait notext 里面有： timeout=3000
cmd /t 0

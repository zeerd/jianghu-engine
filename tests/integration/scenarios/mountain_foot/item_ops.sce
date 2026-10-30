# item_ops.sce — 卸下武器 + 丢弃物品（un_wield / drop 的本人版回执）
#
# 覆盖：
#   CCmdStandard::un_wield 的单件卸下分支（L1 已在 wield_sword.sce 把铁剑
#     装到右手，unwield_msg 取自 item/sample_sword_basic.lua）
#   CCmdStandard::drop + do_drop 的成功分支（丢到当前房间）
#
# 判定：
#   · 卸下回执「你将铁剑插回剑鞘。」是 message_vision 的本人版
#     （模板 $N将手中的$n插回剑鞘。→ $N 渲染为「你」），房间版带名字，
#     同房别的账号不会误满足；
#   · 丢弃回执「你向山脚茶棚里丢下数量1的茶壶。」同为本人版，且茶棚只本道
#     会来（其余各道不去茶棚），无串扰；
#   · 茶壶带 value=50，不会走 do_drop 里「并不值钱」的销毁分支，回执唯一。
#
# 前置：铁剑取自山脚木箱并已装备（wield_sword.sce）、茶壶购自商人并已在
#   茶棚装水饮用（buy_sell.sce / tea_fill_drink.sce）；本段放在茶棚段之后，
#   丢弃的正是用完的茶壶，不碰后续房屋段要用的金创药。
#
# ⚠ 目标复位：两个命令都靠 /t 定向，块末必须 /t 0（§8.5）。

cmd /t sample_sword_basic
clear
cmd /unwield
wait text 你将手中的铁剑插回剑鞘 timeout=10000

cmd /t cha hu
clear
cmd /drop
wait text 你向山脚茶棚里丢下数量1的茶壶 timeout=10000
# 状态复核：茶壶已从背包落到房间，`object` 条件（房间内对象）出现才算真移出
wait object 茶壶 timeout=10000
cmd /t 0

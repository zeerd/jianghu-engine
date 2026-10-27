# wield_sword.sce — 装备武器：wield 到右手
#
# 覆盖：CItem::wield 装备链路。
#
# 判定（两层，缺一层都算没测到位）：
#   · 装备回执「抽出一柄」（item/sample_sword_basic.lua 的 wield_msg）只说明
#     动作发生了——回执文本里没有任何槽位信息；
#   · 再用 `wield 铁剑 右手` 断言物品真的落进了右手（铁剑的
#     wield_position = WIELD_RIGHT_HAND），槽位由 CLI 侧 inventory 的 wield
#     字段判，不靠文本猜。
#
# 说明：铁剑取自山脚木箱（mountain_foot/open_boxes.sce），故本段只属于 L1。

cmd /t 铁剑
clear
cmd /wield
wait text 抽出一柄 timeout=10000
wait wield 铁剑 右手 timeout=10000
sleep 600
cmd .info
cmd /t 0

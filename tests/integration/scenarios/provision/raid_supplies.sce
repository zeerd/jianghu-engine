# raid_supplies.sce — L8（山贼试炼道）在山脚商人的补给采购
#
#   金创药（商品 5）买 9 粒 —— con30 + 10 级的 eff_hp 上限是 1260（升满后），
#     建档 eff=360 → 9×100 顶满。金创只推 eff_hp 不推 hp（首轮实测），顶 eff
#     是给 hp 划出可回复的天花板。
#   回春丹（商品 6）买 10 粒 —— 战前 9 粒把 hp 360→1260 补满（N4 起 L8 不再
#     请教，10 粒为战前 9 + 1 余量；N2 时期的请教链零药直连说法已随 force 链
#     迁 L9 供能作废），战斗中不吃药——战斗内 /use 需 50 AP（实测「需要50点AP，
#     你的AP不足」），裸窗口对败退已够（见 raid_assault 头注）。
#     ⚠ 教训（首轮）：金创不补 hp，不先顶满回春直接请教会在第 4 次撞 hp<100。
#     不买青锋剑：徒手 unarmed 攻击链是本道战斗形态（2026-11-01 起反震前提
#     作废——反震分支已判不可达列入不测，徒手保留为战斗形态本身的设计）。
#
# 判定：本人版成交回执 + 收尾背包断言；`/t` 前先 wait object（§8.4 同款竞态口径）。

wait object 杂货老板
cmd /t huo ji
cmd /list

repeat 9
clear
cmd /buy 5
wait text 你从杂货老板那里买下了数量1的金创药 timeout=10000
end

repeat 10
clear
cmd /buy 6
wait text 你从杂货老板那里买下了数量1的回春丹 timeout=10000
end

wait item 金创药 timeout=10000
wait item 回春丹 timeout=10000
cmd /t 0

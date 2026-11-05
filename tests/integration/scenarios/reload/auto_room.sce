# auto_room.sce — 房间实例刷新：两个刷新通道各验一头
#
#   · 【房景乙】走函数闭包通道：do_look 是脚本表里的闭包，换 ref 即生效；
#   · 【长描述乙】走 dbase 状态通道：long 是 create() 写下的状态，只有
#     重载桥回调 reset(self)（夹具房间定义了它）才刷新；
#   · 【共甲】钉住共享模块未被牵动（common 变体此时还是 A 版）。
# clear 在 filecopy 之前，理由同 auto_npc.sce。

clear
filecopy $fixtures/b/room/sample_mountain_foot.lua $scripts/room/sample_mountain_foot.lua
wait text 自动重载完成：sample_mountain_foot timeout=20000

cmd /t 0
clear
cmd /look
wait text 【房景乙】 timeout=10000
wait text 【长描述乙】 timeout=10000
wait text 【共甲】 timeout=10000

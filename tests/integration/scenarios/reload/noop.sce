# noop.sce — mtime 变、内容不变 → 不产生重载（探针）+ 同拍真实变更做屏障
#
# 「什么都没发生」没有广播可等：让探针原样重写与一次真实变更（房间 B→A
# 回切）同拍写入，等后者的成功广播即证明扫描周期已越过探针——探针未被
# 误重载由 reload_testbed.ps1 断言日志无
# "Lua auto-reload: e2e_reload_noop_probe"。
#
# 房间回切同时验证：真实变更的断言链在回切后依然成立（房景/长描述回甲）；
# 共享模块仍是 B 版（require 缓存于上次 reload all，common 文件未再动）。

clear
filecopy $fixtures/a/room/e2e_reload_noop_probe.lua $scripts/room/e2e_reload_noop_probe.lua
filecopy $fixtures/a/room/sample_mountain_foot.lua $scripts/room/sample_mountain_foot.lua
wait text 自动重载完成：sample_mountain_foot timeout=20000

cmd /t 0
clear
cmd /look
wait text 【房景甲】 timeout=10000
wait text 【长描述甲】 timeout=10000
wait text 【共乙】 timeout=10000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

# reload_all_error.sce — reload all 中一个脚本出错：回执点名失败数、其余不失效
#
# 坏 npc + `reload all` → 回执带失败计数（如实上报：1 个失败）；其余脚本
# 照常重载（房间标记、共享模块都可断言），坏脚本保留旧版本（询问仍丙）。
# 片尾恢复合法变体并等成功广播——错误态清干净，后续片段才有失败边沿可用。

clear
filecopy $fixtures/broken/npc/syntax_error.lua $scripts/npc/e2e_reload_npc.lua
cmd /reload all
wait text 1 个失败 timeout=10000

# 一个坏文件没有拖垮全局：房间与共享模块照常工作
cmd /t 0
clear
cmd /look
wait text 【房景乙】 timeout=10000
wait text 【长描述乙】 timeout=10000
wait text 【共甲】 timeout=10000

cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC丙】 timeout=10000
cmd /t 0

# 片尾恢复合法变体：成功广播到达即错误态已清。用 d 变体——当前内容哈希
# 是 c（cmd_ok 写入），再写 c 会被 noop 分支吞掉、等不到广播。
clear
filecopy $fixtures/d/npc/e2e_reload_npc.lua $scripts/npc/e2e_reload_npc.lua
wait text 自动重载完成：e2e_reload_npc timeout=20000

# shared_base.sce — 共享模块（require 缓存）语义
#
# 自动重载只换 common 自己的脚本表；房间实例 do_look 闭包里的 `common`
# 是加载房间脚本时 require 进来的旧表——单个 reload 与自动重载都不清
# package.loaded（设计现状：command/README 亦要求共享改动走 reload all），
# 故行为保持共甲。`reload all` 清 package.loaded → 房间脚本重跑 → 重新
# require 拿到共乙。
#
# clear 在 filecopy 之前，理由同 auto_npc.sce。

clear
filecopy $fixtures/b/room/e2e_reload_common.lua $scripts/room/e2e_reload_common.lua
wait text 自动重载完成：e2e_reload_common timeout=20000

# 自动重载之后：require 缓存未清，look 仍是共甲
cmd /t 0
clear
cmd /look
wait text 【共甲】 timeout=10000

# reload all 清缓存 → 房间脚本重跑 → 重新 require 拿到共乙
clear
cmd /reload all
wait text 所有 Lua 脚本已重载。 timeout=10000

clear
cmd /look
wait text 【共乙】 timeout=10000
wait text 【房景乙】 timeout=10000

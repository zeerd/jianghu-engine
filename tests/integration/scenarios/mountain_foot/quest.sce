# quest.sce — 任务（通用挂点 pre_talk / pre_accept_object，见 data/quest.lua）
#   没接委托就交物 → 交物钩子不接管，引擎默认拒收
#   接委托 → 交物被钩子接管（物品真的移交给老李头）→ 复命领赏
#   任务状态记在角色 dbase 的 quest/lilaotou 上，随角色存档走
#
# 说明：每个账号各有自己的 dbase，任务状态互不相干，故各道都可独立跑；
#       本道用自己购买的金创药交物，不依赖共享资源。

cmd /t 老李头
clear
cmd /give 金创药
wait text 对方不要你的东西 timeout=10000
sleep 600
clear
cmd /ask 委托
wait text 买一粒金创药来 timeout=10000
sleep 600
clear
cmd /give 金创药
wait text 接过金创药 timeout=10000
sleep 600
clear
cmd /ask 复命
wait text 老朽的一点心意 timeout=10000
sleep 600
cmd /t 0

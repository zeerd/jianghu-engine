# betray_and_repent.sce — 山顶判师 → 半山腰悔过（黑风寨总教头 / 青云子）
#
# 覆盖：
#   l_remove_betrayer（CharLevel.cpp 的 Remove_Betrayer 绑定）——青云子「悔过」
#   菜单第一支清除判师记录（引擎侧代价：按判师次数把全部武功每重降 20 级）。
#   判师的置位面教材原本是空的（expell 不写 betrayer），由黑风寨总教头的 do_talk
#   「判师」补：release_master（顺带消 l_release_master）+ 清家门 + betrayer
#   记账 + 挂 pending/recruit；顺带覆盖 recruit 的 pending 成功支（入伙回执）。
#
# 判定（全私发，§8.6①）：
#   · 判师三回执按因果落缓冲：总教头的 say → recruit 的「请与其交谈」tell →
#     /apprentice 的「恭喜您成为黑风寨的第 2 代弟子」tell（总教头是第 1 代，
#     recruit 给新徒分第 2 代）；
#   · 悔过两支成对成反：第一支「判师记录已消」只在 Remove_Betrayer 调通后才发
#     （脚本中途报错则 handle_action 中断、该行永不出现），第二支「并无判师记录」
#     证明记录真已清空——置位与清除各得一证，不依赖 DSL 读不到的 dbase。
#
# 命令闸说明：改换门派的拒绝在命令层（apprentice 拒「未经本派师门同意不能改换
#   门派」），attempt_apprentice 钩子轮不到，所以判师走 do_talk 问话入口；入伙
#   反向利用 pending/recruit：/apprentice 这次直走成功支。
#
# 并行与道位（§8.4③）：
#   · 本片段是本道（L4）道尾：悔过降技能/清映射之后无人再消费技能与家门状态；
#     判师只改本道角色自己的 dbase，青云子的 attempt_apprentice 主链与别道的
#     拜师互不相干；
#   · 山顶同房有 L3（练功断言全本人版）与 L8（只按房间菜单闯副本）——本段不
#     /look（顶部 do_look 才弹「闯入山寨」菜单，不看就不弹、无菜单吞咽面），
#     也不碰青蛇（L3 稀缺资源）；半山腰同房的 L2 只认铁人桩/技能串。本段全是
#     总教头与悔过的专属私发文本，双向都串不进；新 NPC 无 chat_chance，不产
#     房间噪音。
#
# 顺序：放 tune.sce 之后的道尾——链后早放会踩 npc_master_family_user（家门
#   查询）、sleep_and_observe（同门关系分支）、poison_and_cure（毒术）三段。
# 动线：山脚只通半山腰、半山腰才通山顶，逐跳走；判师（山顶）必须先于悔过
#   （半山腰）——悔过第一支以判师置位为前提。

cmd /t 0
wait not_busy timeout=10000
wait room 青云山山脚 timeout=10000

# --- 山顶：判师（黑风寨总教头），do_talk 问话入口（命令闸见头注）------------
cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=15000
cmd /go 青云山山顶
wait room 青云山山顶 timeout=15000
wait object 黑风寨总教头 timeout=10000

cmd /t hei feng zhai zong jiao tou
clear
cmd /ask 判师
wait text 先与原师门了断 timeout=10000
wait text 请与其交谈 timeout=10000

# 入伙：pending/recruit 已由判师挂上，/apprentice 直走成功支
cmd /apprentice
wait text 恭喜您成为黑风寨 timeout=10000
cmd /t 0

# --- 半山腰：悔过（青云子），第一支清判师记录（l_remove_betrayer 命中）-------
cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=15000
# 房间名状态先于对象名录到达，/t 抢跑会撞「Target not found」——先等名录（同
#   common/apprentice.sce 的 wait room → wait object → /t 三连）
wait object 青云子 timeout=10000
cmd /t qing yun zi
clear
cmd /ask 悔过
wait menu timeout=10000
menu 洗去判师之名
wait text 判师记录已消 timeout=10000

# --- 悔过第二支：记录已空，同菜单改走拒绝支（状态反向收口）-------------------
cmd /ask 悔过
wait menu timeout=10000
menu 洗去判师之名
wait text 并无判师记录 timeout=10000
cmd /t 0

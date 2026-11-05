# persist_pre.sce — 挑战计数跨重启验证·试验台第一段
# （run-integration-tests.ps1 的 persist 试验台，与各道并行执行）
#
# 业务：黑风寨主（sample_bandit_boss.lua 的 can_save 存盘演示）在玩家进房时触发
#   其 init → 挑战计数 +1 → Save 落 t_npc → 喊话「你已经是第N个来送死的了！」
#   → 主动开战。本段拿到喊话即结束（不打完、不逃跑）：计数在喊话前已同步落库，
#   试验台任务随即停服重启，重启后的 persist_post 断言 N+1。
#
# 剧本结构的依据（副本实例语义，改片段前先核对）：
#   · 每次「闯入山寨」load_fb 都新建副本实例——房间全新、spawned=0；
#   · 大厅 boss 由**房间 init 在首次进厅时刷出**，而 on_enter 先遍历既有对象、
#     后跑房间 init——首进时 boss 尚不存在，其 init 不会触发；
#   · 故须**同实例回环**：首进刷出 → 步行出厅 → 再进（boss 已在房）→ 对象
#     init 触发。出厅走门（/go 庭院大门，按门 id 匹配）：大厅 valid_leave 对
#     aggressive 的拦截用 query 比较字符串，而 query 对字符串 dbase 值恒返回
#     0，实际恒放行——本段依赖此现状；若该拦截将来被修好，本段会以
#     「还有敌人在场」超时暴露，届时改走副本管理员离开菜单并重新设计回环。
#
# 断言：wait text 扫累积缓冲（喊话先于 wait 到达也不漏）；二进后玩家会被开战
#   移进战斗房，故不 wait 大厅房间状态，只断言喊话与战斗状态。

include ../common/login.sce

# --- 走位到黑风寨（山脚无直达山顶的门，须经半山腰中转，与 L3/L8 同款） --------
cmd /go 青云山半山腰
wait room 青云山半山腰
cmd /go 青云山山顶
wait room 青云山山顶
cmd /look
wait menu
menu 闯入山寨
wait room 黑风寨庭院

# --- 首进大厅：房间 init 刷出大当家（本次对象 init 不触发，玩家留在大厅） ------
cmd /go 聚义大厅
wait room 黑风寨大厅
wait object 黑风寨主

# --- 同实例回环：出厅再进 → 触发挑战计数、喊话、主动开战 ----------------------
cmd /go 庭院大门
wait room 黑风寨庭院
cmd /go 聚义大厅
wait text 你已经是第 timeout=30000
capture 你已经是第 as n
wait battle

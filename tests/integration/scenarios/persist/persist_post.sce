# persist_post.sce — 挑战计数跨重启验证·试验台第二段
# （persist 试验台在段 1 之后重启服务器，本段跑在新实例上；与各道并行）
#
# 前置：persist_pre 已在重启前触发过一次挑战计数并 capture n；试验台任务已
#   重启服务器、把 expect_n=n+1 注入本段 vars。
#
# 业务：重启后黑风寨主 create() 末尾的 self:Load() 应从 t_npc 恢复挑战计数，
#   本段再触发一次 init → 计数应变为 n+1——喊话出现「第${expect_n}个」即证明
#   计数跨重启继续累加（若 Load 失效，计数会从 0 起算、喊「第1个」，本段超时
#   暴露）。
#
# 副本实例语义与回环依据同 persist_pre 头注（每次闯入山寨都是全新实例、
# 首进刷出不触发对象 init、出厅依赖 valid_leave 的 query 字符串拦截恒放行）。

include ../common/login.sce

# --- 走位到黑风寨 ------------------------------------------------------------
cmd /go 青云山半山腰
wait room 青云山半山腰
cmd /go 青云山山顶
wait room 青云山山顶
cmd /look
wait menu
menu 闯入山寨
wait room 黑风寨庭院

# --- 首进大厅：重启后房间全新，本次刷出大当家（Load 已在 create 时恢复计数） --
cmd /go 聚义大厅
wait room 黑风寨大厅
wait object 黑风寨主

# --- 同实例回环：出厅再进 → 触发计数，喊话必须是 expect_n（跨重启 +1） --------
cmd /go 庭院大门
wait room 黑风寨庭院
cmd /go 聚义大厅
wait text 你已经是第${expect_n}个 timeout=30000
wait battle

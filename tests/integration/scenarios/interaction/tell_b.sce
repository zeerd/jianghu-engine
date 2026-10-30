# tell_b.sce — 私聊与屏蔽（B 方：收悄悄话、屏蔽、恢复，覆盖 refuse_tell/listen）
#
# 与 tell_a.sce 配对并行。松同步：
#   B 收「悄悄话甲」→ 屏蔽 A 并回执「已屏蔽」→ 等 A 的房间版 say「解除屏蔽」
#   → 恢复收听并回执「已恢复」→ 收「悄悄话丙」。

# 1) B 收到 A 的悄悄话甲
wait text 悄悄话甲 timeout=20000

# 2) B 屏蔽 A（refusetell），回执 A（B→A tell 不受屏蔽方向影响）
cmd /t $peer_acct
cmd /refusetell
wait text 你拒绝收听 timeout=10000
cmd /tell 已屏蔽

# 3) B 等 A 的房间版 say，恢复收听（wantlisten），回执 A
wait text 解除屏蔽 timeout=25000
cmd /wantlisten
wait text 你开始收听 timeout=10000
cmd /tell 已恢复

# 4) B 收到 A 的悄悄话丙
wait text 悄悄话丙 timeout=25000
cmd /t 0

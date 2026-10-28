# party_a.sce — 组队全链（A 方：邀请者/队长，覆盖 CmdUserParty 邀请/查队/踢除）
#
# 与 party_b.sce 配对并行。松同步点：
#   A `/party` 邀请 → B 菜单接受 → A 私发「接受了你的组队邀请」。
#   ⚠ 关键同步：A 的 partylist/错误分支必须在 B 仍在队中时完成，而 B 接受后
#     会立刻跟随+离队——两者并行会竞态（B 先离队 → A partylist 报「你并没有
#     处于队伍中」）。故 A 在查队+错误分支后私发信号「可以离队了」，B 收到
#     才开始跟随+离队（见 party_b step2）。
#   错误分支（accept_party 非邀请者）放到组队解散之后由 B 单独触发，避免竞态。

# 邀请组队：A 邀请 B（B 收到菜单并接受）
cmd /t $peer
cmd /party
wait text 接受了你的组队邀请 timeout=20000
cmd /t 0
sleep 400

# partylist：队长查在线队员（私发列表，含成员 B）——B 此刻仍在队中（等信号）
cmd /partylist
wait text 你所处队伍中有以下成员 timeout=10000
wait text 成员 timeout=10000
sleep 400

# 错误分支：已在队中再邀请 B → 对方已经接受了你的邀请（A 私发 notify_fail）
cmd /t $peer
cmd /party
wait text 对方已经接受了你的邀请 timeout=10000

# 同步信号：告知 B 查队/错误分支已完成，可以开始跟随+离队
cmd /tell 可以离队了
cmd /t 0
sleep 400

# 等 B 完成跟随开关与离队；A 收到 B 离队私发播报
wait text 取消了与你的组队 timeout=30000
sleep 400

# 重新组队恢复：A 再邀请 B，B 菜单接受
# clear 清掉首次组队留下的旧「接受了」，否则下面 wait 立即假匹配、A 不等 B
# 真接受就去踢 → 踢时 B 尚未归队而失败
clear
cmd /t $peer
cmd /party
wait text 接受了你的组队邀请 timeout=20000
cmd /t 0
sleep 400

# partykick：A 踢除 B（A 侧 message_vision 本人版「你取消与…」）
cmd /t $peer
cmd /partykick
wait text 你取消与 timeout=10000
cmd /t 0
sleep 400

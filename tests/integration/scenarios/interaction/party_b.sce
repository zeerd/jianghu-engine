# party_b.sce — 组队全链（B 方：接受者/跟随者，覆盖 accept_party/跟随/离队/错误分支）
#
# 与 party_a.sce 配对并行。松同步点：
#   B `wait menu` 等 A 的邀请菜单 → `menu 接受` → 私发对话「你接受了组队邀请」。
#   ⚠ step2：B 接受后**先等 A 的私发信号「可以离队了」**（A 已完成查队+错误分支），
#     才开始跟随+离队——否则与 A 的 partylist 竞态（B 先离队会让 A 查队失败）。
#   之后 A 重新组队→B 再接受；A 踢除→B 收私发播报；最后触发 accept_party 错误分支。

# 等 A 的邀请菜单 → 接受
wait menu timeout=25000
menu 接受
wait text 你接受了组队邀请 timeout=10000
sleep 400

# 等 A 完成 partylist + 错误分支后的同步信号（期间 B 保持在队中不动）
wait text 可以离队了 timeout=30000

# partyfollow 开启/取消（B 私发对话：决定跟随 / 取消跟随）
cmd /t $peer
cmd /partyfollow
wait text 决定跟随 timeout=10000
cmd /partyfollow
wait text 组队跟随 timeout=10000
sleep 400

# partyleave：B 主动离队（B 私发对话「你取消了与…的组队」）
cmd /t $peer
cmd /partyleave
wait text 你取消了与 timeout=10000
cmd /t 0
sleep 400

# A 重新组队：B 再次菜单接受
# clear 清掉首次接受留下的旧「你接受了」，否则下面 wait 立即假匹配（菜单/接受
# 用 menu_state 与新回执，不受 clear 影响）
wait menu timeout=25000
clear
menu 接受
wait text 你接受了组队邀请 timeout=10000
sleep 400

# A 踢除 B：B 收到私发播报「…取消了与你的组队」（B 自己离队产生的是「你取消了与」，不冲突）
wait text 取消了与你的组队 timeout=20000
sleep 400

# 错误分支：对未邀请者 accept_party → 对方并没有邀请你（此时 A 已解散队伍）
cmd /t $peer
cmd /accept_party
wait text 对方并没有邀请你 timeout=10000
cmd /t 0
sleep 400

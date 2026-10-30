# pair_b.sce — 夫妻组队（B 方：关邀请菜单 / accept_pair / pairfollow / pairleave 错误分支）
#
# 与 pair_a.sce 配对并行。⚠ A 的 /pair 菜单里是 accept_party（不带夫妻标记），
#   而「拒绝组队邀请」项命令为空、CLI 侧直接丢弃（game_state_question.cc 只收
#   非空 raw_cmd），菜单里只有一项接受——故 B 只等菜单出现（= 邀请已生效），
#   再用非数字 cmd（/t）隐式关掉菜单（process_line 对 in_menu 的非数字输入
#   先 cancel_menu 再继续），最后直发 /accept_pair 覆盖夫妻版接受函数。

# 等 A 的夫妻组队邀请菜单（到达即证明 A 的 /pair 已生效）
wait menu timeout=25000

# 直发 accept_pair（要求双方 couple 互指；接受后 party/couple 置位）
cmd /t $peer_acct
clear
cmd /accept_pair
wait text 你接受了组队邀请 timeout=10000

# pairfollow：跟随开启（B 私发对话「你决定跟随…」）
cmd /pairfollow
wait text 决定跟随 timeout=10000
cmd /t 0

# A pairkick 踢除 B：B 收私发播报「<A名>取消了与你的组队」
wait text 取消了与你的组队 timeout=25000

# pairleave 错误分支：已被踢出、party/master 已清 → 你并没有加入别人的队伍
# 这里**不加 clear**：A 的离婚推进可以快到在本片段收尾前就广播
# 「<A名>与你解除了夫妻关系。」，而 divorce_b 的收敛 wait 在本片段之后——
# 中途任何 clear 都会把提前到达的跨道播报抹掉（round9 实锤竞态：
# 播报落点早于本块、被抹后 divorce_b 干等超时，A/B/L7 三道连锁红）。
# 本块 wait 的 needle 只可能由本命令自身产生，不存在旧输出假匹配。
cmd /t $peer_acct
cmd /pairleave
wait text 你并没有加入别人的队伍 timeout=10000
cmd /t 0

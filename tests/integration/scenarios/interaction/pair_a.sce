# pair_a.sce — 夫妻组队（A 方：pair/pairlist/pairkick，覆盖 CmdUserParty 的 pair* 系）
#
# 前置：marriage_a/b.sce 已建立 couple（pair 入口要求双方 couple/id 互指 +
#   等级差 ≤ couple_level_diff=10，新号同级满足）。
#
# ⚠ pair 的邀请菜单里是硬编码的 `$0accept_party`（party() 复用），走菜单接受
#   不会置 party/couple、也不覆盖 accept_pair——故 B 侧关掉菜单后直发
#   /accept_pair（见 pair_b.sce）。

# A 发起夫妻组队（A 本人版 message_vision「你热情的对<peer>说」）
cmd /t $peer_acct
clear
cmd /pair
wait text 热情的对$peer说 timeout=10000
cmd /t 0

# B 直发 accept_pair 后，A 收到 message_vision 的 you 视角「<B名>接受了你的组队
#   邀请。」（party_a.sce 同款断言；「你接受了组队邀请」是 B 侧 say 文本）
wait text 接受了你的组队邀请 timeout=25000

# pairlist：队长查在线队员（私发列表，成员含 B）
clear
cmd /pairlist
wait text 你所处队伍中有以下成员 timeout=10000

# pairkick：A 踢除 B（A 侧 message_vision 本人版「你取消与…」）
#   B 随后 pairleave 落错误分支（已被踢出、无队长）
cmd /t $peer_acct
clear
cmd /pairkick
wait text 你取消与 timeout=10000
cmd /t 0

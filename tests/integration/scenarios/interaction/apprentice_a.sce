# apprentice_a.sce — 玩家收徒·场景 B（B 先发起，A 以 /recruit 兑现）
#
# 与 apprentice_b.sce 配对并行，段落位置：expell_a 之后（关系已清、双方仍在
#   山脚茶棚）。链路：B 的 /apprentice 置 pending/apprentice + 私发 A →
#   A 的 /recruit 命中 pending → CChar::recruit_apprentice 重建关系——与场景 A
#   同一条建立路径，只是发起方向相反（期间同样不能换房，见 recruit_a 头注）。
# 断言：
#   ·「如果你愿意收<peer>为弟子」——B 发起的私发（场景 A 不会走到这句）；
#   · clear 后断「你决定收<peer>为弟子」（本人版播报）与「恭喜你新收了一名弟子」
#     ——后者场景 A 已出现过，不清会假匹配；clear 落在消费完 B 的发起信号之后，
#     B 此后再不发任何信号（§8.6⑦ 安全）；
#   · 状态复核：/look B →「是你的弟子」（场景 A 的旧回执已被上面的 clear 清掉，
#     断的是本次建立）。
# family/user=1 的证据沿场景 A/D/E（同一条 recruit_apprentice 路径），本段不重复
#   存盘，省一次全量落库停顿。

# B 发起（场景 B）：先等到信号再 clear（clear 不得落在信号到达之前，§8.6⑦）
wait text 如果你愿意收$peer为弟子 timeout=30000

clear
cmd /t $peer_acct
cmd /recruit
wait text 你决定收$peer为弟子 timeout=10000
cmd /t 0
wait text 恭喜你新收了一名弟子 timeout=10000

# 状态复核（同场景 A）：定向观察 B →「是你的弟子」
cmd /t $peer_acct
cmd /look
wait text 是你的弟子 timeout=10000
cmd /t 0

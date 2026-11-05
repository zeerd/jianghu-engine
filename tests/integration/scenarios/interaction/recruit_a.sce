# recruit_a.sce — 玩家收徒·场景 A（A 发起 /recruit，B 以 /apprentice 兑现）
#
# 与 recruit_b.sce 配对并行，段落位置：party 之后（双方同在山脚茶棚，party 不挪窝）。
#   ⚠ 为什么不在会合之后：/party 会向被邀请方推**菜单**，而任何非数字输入的 cmd
#   都会 cancel_menu（input.cc）——对端若正执行 cmd，菜单被吞、其 `wait menu` 必红。
#   师徒段只产生文本信号（房间播报/私发）与「观察者自己」的菜单（/look 的菜单只
#   发给发起观察的一方），故必须排在全链唯一的对端推菜单段（party）之后。
# 链路：CCmdStandard::recruit 置 pending/recruit + 房间播报 + 私发 B →
#   B 的 CCmdStandard::apprentice 命中 pending → CChar::recruit_apprentice 建立关系。
#   pending 是 temp：任一方 go 换房都会被 delete_temp("pending") 清掉，
#   故本段到建立为止双方都不动（同房间零通信之外无任何位移）。
#
# 断言分层（§8.4① 状态优先）：
#   · 本人版/私发回执：「你想要收<peer>为弟子。」（房间播报的本人版）、
#     「恭喜你新收了一名弟子！」；
#   · 状态：/look B →「是你的弟子」——CChar::do_look 的 family_des 只在
#     门派相同 + family/master_id == 对方 id + 代数差 1 同时成立时才渲染，
#     一条断言钉住 master_id / family_name / generation 三项；
#   · family/user=1 的证据在 rexp_a.sce（「(弟子)」标记只由 CharSave 的
#     family/user 分支写入）与 expell_a.sce（开革后翻回「(----)」）。
#
# ⚠ /t 定向纪律：本段 /t 只用 $peer_acct，块尾复位 /t 0（§8.5）。
# ⚠ 本段全程不 clear（§8.6⑦）：B 侧建立后还要发「徒弟存好了」，
#   clear 落在它到达之前就会把信号吞掉且播报不重发。

cmd /t $peer_acct
cmd /recruit
wait text 你想要收$peer为弟子 timeout=10000
cmd /t 0

# B 以 /apprentice 兑现 → A 收建立回执（私发）
wait text 恭喜你新收了一名弟子 timeout=30000

# 状态断言：定向观察 B（同房间）→ family_des 的「是你的弟子」
cmd /t $peer_acct
cmd /look
wait text 是你的弟子 timeout=10000
cmd /t 0

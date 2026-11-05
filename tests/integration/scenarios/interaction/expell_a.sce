# expell_a.sce — 开革玩家徒弟 /expell（场景 E）
#
# 前置：场景 A/D 完成（师徒关系在、recruit_exp 表里有这行记录）。
# 链路：CCmdStandard::expell 的 is_apprentice_of 分支（A 非掌门 → family/privs
#   != -1，走「你我师徒恩断情绝」那支）→ CChar::release_master → 记录的弟子
#   标记翻回 0；随后 del family/class、title 置弃徒、存盘。
# 断言：
#   · 房间播报本人版「你对着<peer>说道：从今天起，你我师徒恩断情绝」；
#   · 再查 /rexp 得「(----)」——release_master 在 family/user==0 时早退，
#     标记会停在「弟子」；本条与 rexp_a 的「(弟子)」合起来双向钉住 family/user=1。
#
# ⚠ 本段全程不 clear（§8.6⑦）：B 侧收开革回执后立刻发场景 B 的重拜信号，
#   clear 落在它到达之前会把信号吞掉且播报不重发。发起信号的等待在
#   apprentice_a（场景 B 首步），中间没有任何 clear。

cmd /t $peer_acct
cmd /expell
wait text 你对着$peer说道：从今天起，你我师徒恩断情绝 timeout=10000
cmd /t 0

cmd /rexp
wait text 徒弟$peer($peer_acct)贡献经验：0。(----) timeout=10000

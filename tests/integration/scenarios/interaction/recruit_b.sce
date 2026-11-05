# recruit_b.sce — 玩家收徒·场景 A（B 侧：收 A 的 /recruit、以 /apprentice 兑现）
#
# 与 recruit_a.sce 配对并行（段落位置：party 之后，理由见 recruit_a 头注）。B 的
#   /apprentice 命中 A 置下的 pending/recruit
#   → 直接走建立分支（「你想要拜…为师」那句在未兑现分支，本场景不打印）。
# 断言：
#   ·「如果你愿意拜<peer>为师父」——A 的 /recruit 私发 B（§8.6①）；
#   ·「你决定拜<peer>为师」——建立播报的本人版（B 自己跪拜那句）；
#   ·「恭喜您成为青云门的第3代弟子」——建立回执，含门派与代数
#     （recruit_apprentice 写入 family_name=青云门、generation=师父代数 2 + 1）；
#   · 状态：/look A →「是你的师父」（与 recruit_a 同一 family_des 判据，反向）；
#   · 存盘：CharSave::save_to_server 的 family/user 分支写师徒经验记录，
#     rexp_a 的 /rexp 依赖这行落库（g_channel.query_recruit 直读 recruit_exp 表）。
#
# ⚠ /t 定向纪律同 recruit_a；本段各回执均为首次出现，无需 clear。

wait text 如果你愿意拜$peer为师父 timeout=30000

cmd /t $peer_acct
cmd /apprentice
wait text 你决定拜$peer为师 timeout=10000
wait text 恭喜您成为青云门的第3代弟子 timeout=10000
cmd /t 0

# 状态断言：定向观察 A →「是你的师父」
cmd /t $peer_acct
cmd /look
wait text 是你的师父 timeout=10000
cmd /t 0

# 存盘落库（family/user=1 才写；B 此时尚未历练，贡献经验为 0）
cmd /save
wait text 档案储存完毕 timeout=30000

# 回执 A：rexp_a 的 /rexp 以它为前置（§8.6① 私发信号）
cmd /t $peer_acct
cmd /tell 徒弟存好了
cmd /t 0

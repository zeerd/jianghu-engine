# apprentice_b.sce — 玩家收徒·场景 B（B 侧：先发起 /apprentice，等 A 兑现）
#
# 与 apprentice_a.sce 配对并行（段落位置：party 之后、tell 之前，总理由见
#   recruit_a 头注——本段尾部有 `cmd /t 0`，若此时对端正推菜单过来就会被它
#   cancel 掉，故下一片段必须是纯文本信号的 tell_b）。发起走未兑现分支：B 私发「你想要拜…为师」并置
#   pending/apprentice，A 收「如果你愿意收…为弟子」后以 /recruit 兑现。
# 断言：
#   ·「你想要拜<peer>为师」——本段首次出现（场景 A 走兑现分支，不打印这句）；
#   ·「恭喜您成为青云门的第3代弟子」——场景 A 已出现过，须先 clear 才不假匹配；
#   · 状态复核：/look A →「是你的师父」（clear 已把场景 A 的旧回执清掉）。
#
# ⚠ clear 的位置（§8.6⑦）：必须落在 /apprentice **之前**——发起之后 A 的兑现
#   回执随时会到，clear 在中间会把它吞掉且播报不重发。此刻 A 正在 expell_a /
#   apprentice_a 等我发起，两侧的开革播报都已被 expell_b 消费完，无在途信号。

clear
cmd /t $peer_acct
cmd /apprentice
wait text 你想要拜$peer为师 timeout=10000
wait text 恭喜您成为青云门的第3代弟子 timeout=10000
cmd /t 0

# 状态复核（同场景 A）：定向观察 A →「是你的师父」
cmd /t $peer_acct
cmd /look
wait text 是你的师父 timeout=10000
cmd /t 0

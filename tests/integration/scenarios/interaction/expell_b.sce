# expell_b.sce — 开革玩家徒弟（B 侧：收开革回执）
#
# 与 expell_a.sce 配对并行。两条断言分属两类通道（§8.6②）：
#   · 房间版含对方名渲染「<peer>对着你说道：从今天起…」（message_vision 的
#     other 侧，$N 渲染成对方名、$n 渲染成「你」）；
#   · 私发「你被师父开革出师门了！」——expell 的 is_apprentice_of 分支文案；
#     掌门分支文案是「你被开革出师门了」，A 非掌门不会走到，故本条也顺带
#     钉住「走的是师徒分支」。
# 本段之后 B 立刻发场景 B 的重拜（apprentice_b 首步先 clear 再发，
#   此刻 A 已消费完自己的开革播报、不会再发信号，§8.6⑦）。

wait text $peer对着你说道：从今天起，你我师徒恩断情绝 timeout=10000
wait text 你被师父开革出师门了 timeout=10000

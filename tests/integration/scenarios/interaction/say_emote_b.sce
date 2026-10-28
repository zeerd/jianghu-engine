# say_emote_b.sce — 定向说话/表情 玩家版（B 方：断言收到 A 的定向说话与表情）
#
# 与 say_emote_a.sce 配对并行。B 依次等 A 的定向 say 与 face 表情的目标渲染。

# A 的定向说话（B 收「<A名>对你说道：你好呀朋友」，私发本人版）
wait text 对你说道 timeout=20000

# A 的鬼脸表情（B 收目标渲染「<A名>对着你顽皮地做了个鬼脸」）
wait text 鬼脸 timeout=10000
sleep 400

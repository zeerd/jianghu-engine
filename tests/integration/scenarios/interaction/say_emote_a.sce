# say_emote_a.sce — 定向说话/表情 玩家版（A 方：对 B 说话 + 发鬼脸表情）
#
# 与 say_emote_b.sce 配对并行。A 只发不等（fire-and-forget），由 B 侧断言送达：
#   定向 say → B 收「<A名>对你说道：<msg>」（私发，含「你」本人版）；
#   表情 face → B 收目标渲染「<A名>对着你顽皮地做了个鬼脸」。
#   附带覆盖 CChar::relay_say 玩家版（对玩家定向说话会调用，玩家版为空函数，
#   无可断言文案，仅计覆盖率——老李头 NPC 版已由 L2 覆盖）。

cmd /t $peer
cmd /say 你好呀朋友
sleep 300
cmd /face
cmd /t 0
sleep 400

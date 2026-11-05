# channel_and_banword.sce — 频道发言回显 → 新手频道回显 → 社交表情 → 频道禁词
#                        → answer 频道接管（天下大考）
#
# 覆盖：
#   ChatChannel 的发言链路与回显（`.tune new` 是客户端本地切频道，切后纯文本
#     以 new 前缀直发，断言服务端回显「：<内容>」——仅断言「未被拒」不够：
#     发言可能被静默丢弃，无回显也无拒绝文案，故必须看到回显才算过；
#     「年纪太小」年龄门槛（age<15）在 config 的 start_age=16 下不可达，已改由
#     单测 test_channel_deep.cpp 覆盖，需 E2E 低龄分支时另起低 start_age 服务）；
#   表情系统（EmoteSystem 的 myself 版私发 / others 版房间播报）；
#   频道禁词（lua-sample/data/chanbanword → check_channel_msg → lstrchr 逐字匹配）；
#   hooks.do_channel 接管（lua-sample/data/exam.lua 天下大考，answer 频道）：
#     返回数组 →接管广播（考官出题/判分），返回 nil 的其余频道照走内置分支
#     ——上面的新手频道回显就是 fallthrough 的现成证据；答对发历练并
#     FlushMyInfo 推送，用 attr 断真实状态（回显只证明钩子跑了）。
#
# 判定与并行说明：
#   · 「不文明字眼」是拒绝回执，只由本道的纯文本发言触发
#     （其余各道的 cmd 一律以 / 开头，不会走聊天频道）；
#   · 面包屑式的「鬼脸」断言前必须先 clear，否则会用上一次表情的旧回执误满足。
#   · 「：<内容>」回显带本道独有文案，别的账号发不出同样的句子。
#   · answer 频道只由本道发言：广播达全服（登录默认收听 answer），但考官文案
#     独有、其余道不断言它；反向亦然——本道的 wait 只可能被自己的广播满足。

clear
cmd 大家好，我是来测试的。
wait text ：大家好，我是来测试的。 timeout=10000

cmd .tune new
clear
cmd 我在新手频道和大家打招呼。
wait text ：我在新手频道和大家打招呼。 timeout=10000

# ---------------------------------------------------------------------------
# 社交表情：无目标走 myself 版、带目标走 myself_target 版，都是本人私发
# ---------------------------------------------------------------------------
clear
cmd /face
wait text 鬼脸 timeout=10000

cmd /t lao li
clear
cmd /face
wait text 鬼脸 timeout=10000
cmd /t 0

# ---------------------------------------------------------------------------
# 频道禁词：含禁词的发言被服务端直接拒绝，一个字都不广播。
# 断言用拒绝文案本身——它是唯一能证明「查过禁词表」的正向回执
# ---------------------------------------------------------------------------
clear
cmd 我偏要说混蛋二字
wait text 不文明字眼 timeout=10000

# ---------------------------------------------------------------------------
# answer 频道 = hooks.do_channel 接管（天下大考，data/exam.lua）：
#   出题 → 答错 → 答对三步；答对那步先断广播回执、再 attr 断真实历练入账
#   （RTNINFO 由钩子内 FlushMyInfo 先于广播行发出，同 TCP 有序流，
#    wait text 命中时属性已刷新，attr 紧跟即可）。
# ---------------------------------------------------------------------------
cmd .tune answer
clear
cmd 出题
wait text 天下大考第 1 题 timeout=10000

clear
cmd 杜甫
wait text 答错了 timeout=10000

clear
cmd 李白
wait text 答对了 timeout=10000
wait attr combat_exp >= 1000 timeout=10000

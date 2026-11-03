# walk_and_look.sce — 基础命令探针：look 与跨房行走（简单模式专用片段）
#
# ⚠ 仅供人类快速确认（见 lane_simple.sce 头注），AI/Agent 禁用。
#
# 覆盖：
#   · /look → 房间长描述（say(long, me) 私发，只有本人收得到）与房间对象推送；
#   · /go <房间名> → GOTOROOM 状态切换（wait room 是状态断言，只与本人相关）。
#
# 断言纪律（§8.4）：全部用私发文本与本人状态，不依赖任何房间广播。
#   每次 /look 之前都先等「进场输出到齐」再 clear——登录与换房的自动输出（房间
#   描述、ADDOBJECT 推送，后者晚于状态一拍）先被 wait object 兜住，clear 之后
#   的 `wait text` 就只能由这条 /look 的回包满足，look 不可用时不会被进场输出
#   假通过。

# 登录落地（common/login.sce 已断言落在青云山山脚）：先等对象推送到齐再 clear
wait object 老李头
clear
cmd /look
wait text 这里是青云山的山脚 timeout=10000

cmd /go 青云山半山腰
wait room 青云山半山腰
wait object 青云子
clear
cmd /look
wait text 这里是青云山的半山腰 timeout=10000

cmd /go 青云山山脚
wait room 青云山山脚

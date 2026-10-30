# room_look_inquiry.sce — 房间观察 + 向导问路：set_inquiry 文本问答与回调
#
# 覆盖：
#   room/sample_room_common.lua 的 say_long（房间长描述，经 say(long, me) 私发）
#   sample_guide.lua 的 set_inquiry("山") 预置回答（LuaNpc.inl → say(answer, me)
#     私发）与 set_inquiry_func("师父") 回调（发放 50000 历练，按角色的
#     reward/exp 去重，故每道各领一次）
#
# 判定：
#   · 房间描述与「山顶有一伙山贼盘踞」都走私发，只有本人收得到；
#   · 「师父」回调无返回值、其应答是房间播报，不在此断言——历练是否真到账
#     交给下游的「升级成功」判定（upgrade 要求 valid_exp 足够，而升级抬高
#     level2_exp 会把起步历练吃成负数，历练没到账必然过不了那一步）。

cmd /t 0
clear
cmd /look
wait text 这里是青云山的山脚 timeout=10000
wait object 老李头

cmd /t lao li
clear
cmd /ask 山
wait text 山顶有一伙山贼盘踞 timeout=10000

clear
cmd /ask 师父
cmd /t 0

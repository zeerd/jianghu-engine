# exercise_no_raise.sce — 打坐机制补全：「收工时 mp < max_mp×2 不抬升」那条分支
#
# N4 L3 拆分把打坐链整体迁到本道（L10）后，原 L3 sneak 段的「不抬升」覆盖
# （经验果 max_mp 101、长程打坐 mp 121 < 202）随之迁入此处重建——但本道没有
# 经验果（树箱钉给 L3，§8.4③），故改用 L9 巫师 `call set max_mp 100` 造出
# 大上限：mp 2 + 预算 10 = 12 < 2×100 → 收工必然不抬升。
#
# 链条位置：必须排在 halt_exercise **之后**（其断言 max_mp<=1 要求彼时上限
#   仍是 1）、learn_throwing 之前。
#
# 覆盖：
#   · CCmdStandard::exercise 收工的「mp < max_mp×2 → 不抬升」分支（exercise
#     内部 line 369 后的判定跳过路径）；
#   · 巫师 call set 对 max_mp 的直写 + 下一次打坐判定读到新上限——
#     状态断言 max_mp>=100 先钉住直写生效，打坐收工后再断言 max_mp<=100
#     钉住「没抬」（若误抬升即 101，此处红）。
#
# 判定纪律：与 exercise_max_mp/halt_exercise 同——「你坐下来运气用功」「你运功
#   完毕」私发/本人版；上限变化全用 attr 断言，不依赖文案渲染。
#
# 服务协议（§3.3）：本道 /t 1 + tell 请求（请求词/capture 标记全服唯一
#   「练功供能」），L9 固定顺序收请求后 call set max_mp 100 + 私发回执；
#   发完即进 wait；receipt timeout 300s。

# --- 请求 L9 供能大上限（本道无树箱经验果，见头注）---------------------------
cmd /t 1
cmd /tell 练功供能请求，练功供能账号是 $acct
wait text 练功供能完毕 timeout=300000
wait attr max_mp >= 100 timeout=10000
cmd /t 0

# --- 收工不抬升：mp 2 + 10 = 12 < 2×100，上限停在 100 ------------------------
clear
cmd /set exercise 10
wait text 你设置环境变量 timeout=10000
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
wait text 你运功完毕 timeout=60000
# 若判定误走抬升分支，上限会变成 101 → 本断言红
wait attr max_mp <= 100 timeout=10000

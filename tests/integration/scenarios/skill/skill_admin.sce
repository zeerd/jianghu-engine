# skill_admin.sce — enable 之后的技能管理一组：
#   enforce 三态（格式/超限/成功）· skill 自身查看 · validate 身份验证 · abandon 三分支
#
# 位置：enable_practice.sce 之后（force 已 enable，映射非空——
#   「未 enable」拒绝分支在 enable 之前的 enforce_disabled.sce）。
#
# 判定：
#   · enforce 三态全是 notify_fail/tell 私发，本人专属；
#   · skill 成功路径的回执是 RTNSKILL 协议消息（CLI 静默合并技能表、
#     无文本行），故断言取「不出现权限拒绝/未知命令」——证明命令被
#     识别且走通了允许分支；deny 分支的正向文案由 L4 account.sce 覆盖；
#   · validate 成功设 temp 后 abandon 才放行，deny→成功两分支都在此；
#   · abandon 成功的回执走 say→QUESTION 对话渲染（正文可 wait text），
#     其 send_menu 是空菜单（自动取消、无输出），但 say 会置 in_dialog——
#     故 abandon 放本片段最后，其后无 menu 步骤（本片段是 lane_skill 末尾）。
#
# ⚠ 各命令的参数经 wire 切分后落 arg，无需 /t（CLI 自动补 0 占位）。

# ---------------------------------------------------------------------------
# enforce：已 enable → 三态
# ---------------------------------------------------------------------------
cmd /t 0
clear
cmd /enforce
wait text 指令格式：enforce timeout=10000
sleep 400

# force 基础等级约 11 级 → 999 > 等级/2，必撞超限门
clear
cmd /enforce 999
wait text 你只能用0表示不运内力 timeout=10000
sleep 400

clear
cmd /enforce 0
wait text Ok. timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# skill 自身查看：回执是 RTNSKILL 协议（无文本行）→ 断言「无拒绝、被识别」
# ---------------------------------------------------------------------------
clear
cmd /skill
wait notext 你无权查看对方技能|未知的命令 timeout=3000
sleep 400

# ---------------------------------------------------------------------------
# abandon 三分支：先撞「未 validate」deny，再验证身份，
#   之后测「没学过的技能」与成功放弃
# （L2 暗码从未设置 → 单参 /validate 走空暗码校验即过）
# ---------------------------------------------------------------------------
clear
cmd /abandon literate
wait text 请先使用 </validate timeout=10000
sleep 400

clear
cmd /validate $pass
wait text 身份验证通过 timeout=10000
sleep 400

clear
cmd /abandon nosuchskill
wait text 你并没有学过这项技能 timeout=10000
sleep 400

# 成功分支：放弃读书写字（learn_basic 学的），回执经 say→对话渲染
# ⚠ 放最后：say 置 in_dialog，其后不再有 menu/等待交互
clear
cmd /abandon literate
wait text 你决定放弃继续学习读书写字 timeout=10000
sleep 400

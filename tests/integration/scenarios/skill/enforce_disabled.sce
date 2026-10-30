# enforce_disabled.sce — enforce 的前置拒绝分支：force 尚未 enable
#
# 覆盖：CCmdStandard::enforce 的 `query_skill_mapped("force")` 为空 →
#   notify_fail「你必须先 enable 一种内功。」
#
# ⚠ 参数检查（!strlen(arg) → 指令格式）排在 enable 检查**之前**，
#   所以必须带参 `/enforce 1` 才能越过格式门撞上这道门；
#   无参的「指令格式」分支放到 enable_practice 之后的 skill_admin.sce 测。
#
# 时机：已学会 force/force_adv（learn_force 段）但尚未 /enable → 映射表为空；
#   enable_practice 跑过就再也进不来，故本片段必须插在 enable_practice.sce 之前。
#
# 判定：notify_fail 私发回执，仅本人可见，同房半山腰的 L4 不会误满足。

cmd /t 0
clear
cmd /enforce 1
wait text 你必须先 enable 一种内功 timeout=10000

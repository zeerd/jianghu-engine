# enable_practice.sce — 激发高级技能 → enable 列表 → practice/cast 的错误分支
#
# 覆盖：
#   CCmdStandard::enable（含「改用另一种内功，内力必须重新修炼」把 mp 清零的分支）
#   enable 列表回显（特殊技能映射关系）
#   force_adv 的 practice_skill 拒绝分支（只能用请教的来增加熟练度）
#   sword_adv 的 practice_skill 体力/内力门槛
#   force_adv 的 do_heal 施法内力门槛（mp=0 时的失败分支）
# 判定：这一段全部走 tell_object / notify_fail，都仅本人可见，
#   天然免疫同房间其他账号串扰。
#
# 说明：`enable force force_adv` 会把 mp 清成 0，正是后面两条内力门槛断言的前提。

clear
cmd /enable force force_adv
wait text 内力必须重新修炼 timeout=10000
sleep 600

clear
cmd /enable sword sword_adv
wait text Ok. timeout=10000
sleep 600

clear
cmd /enable
wait text 以下是你目前使用中的特殊技能 timeout=10000
wait text sword：sword_adv timeout=10000
wait text force：force_adv timeout=10000
sleep 600

clear
cmd /practice force
wait text 只能用请教的来增加熟练度 timeout=10000
sleep 600

clear
cmd /practice sword
wait text 体力或内力不够 timeout=10000
sleep 600

clear
cmd /cast force heal
wait text 内力不足，无法施展自疗术 timeout=10000
sleep 600
cmd /t 0

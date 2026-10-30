# enable_practice.sce — 激发高级技能 → enable 列表 → practice/cast 的错误分支
#
# 覆盖：
#   CCmdStandard::enable（含「改用另一种内功，内力必须重新修炼」把 mp 清零的分支）
#   enable 列表回显（特殊技能映射关系）
#   force_adv 的 practice_skill 拒绝分支（只能用请教的来增加熟练度）
#   sword_adv 的 practice_skill 体力/内力门槛
#   force_adv 的 do_heal 施法内力门槛（mp=0 时的失败分支）
#   prepare 的双技能组合分支（CmdStandardMaster::prepare → can_prepare_combine →
#   CLuaSkill::valid_combine：脚本无字段时的 CSkill 回落 + hook 未注册的直返）
# 判定：这一段全部走 tell_object / notify_fail，都仅本人可见，
#   天然免疫同房间其他账号串扰。
#
# 说明：`enable force force_adv` 会把 mp 清成 0，正是后面两条内力门槛断言的前提。

clear
cmd /enable force force_adv
wait text 内力必须重新修炼 timeout=10000

clear
cmd /enable sword sword_adv
wait text Ok. timeout=10000

clear
cmd /enable
wait text 以下是你目前使用中的特殊技能 timeout=10000
wait text sword：sword_adv timeout=10000
wait text force：force_adv timeout=10000

# prepare 的组合分支（双技能形）：`/prepare force sword` → analyse_string 拆成
# arg=force / map_to=sword → can_prepare_combine → load_skill(force)->valid_combine(sword)。
# 所有技能都是 CLuaSkill（LuaObjects.cpp 的 create_skill 统一 make_unique<CLuaSkill>），
# 所以函数入口不挑技能类别——force 脚本未定义 valid_combine 字段 → 回落
# CSkill::valid_combine=0 → hook 未注册 → 私发拒绝。
# 前置正好是本段已建立的状态：smap 非空（上面两条 enable）、pmap 为空（本道此前只走过
# learn_basic 的「没有使用任何有效特殊技能」空映射负分支）、force/sword 均已请教。
clear
cmd /prepare force sword
wait text 这两种拳术技能不能组合 timeout=10000

clear
cmd /practice force
wait text 只能用请教的来增加熟练度 timeout=10000

clear
cmd /practice sword
wait text 体力或内力不够 timeout=10000

clear
cmd /cast force heal
wait text 内力不足，无法施展自疗术 timeout=10000
cmd /t 0

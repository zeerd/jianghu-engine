# prepare_combine.sce — 半山腰：空手组合供能 → 双槽激发 → 一次两招实证
#
# 覆盖（N4 增量：空手技能组合「一次两招」）：
#   L9 巫师服务协议的首个真实消费方（§3.3：/t 1 + tell 请求 → 固定顺序
#     wait 收请求 → set_skill 供能 → 私发回执）——set_skill 跨房 find_player
#     路径（CmdWizardSystem::set_skill）
#   CCmdStandard::enable 的 hand / unarmed 两槽（skill_map 新增映射）
#   CCmdStandard::prepare 的**成功**分支：can_prepare_combine → CLuaSkill::
#   valid_combine 脚本字段（hand_adv/unarmed_adv 双侧声明）——与
#   enable_practice.sce 的「无字段回落 0 → 这两种拳术技能不能组合」拒绝分支
#   成对成反，正反两面都有断言
#   CCombatd::fight 的连击分支：无武器且 prepare_count>1 → TYPE_QUICK
#   第二击（action_flag=1 → Attack::build_action_description 的「紧跟着」前缀）
#   ——「紧跟着」是第二击的唯一标识，两技能动作文本同现即一次两招
#
# 前置状态（本道既有）：
#   · sword 已 enable 且铁剑在手（learn_sword 段）→ 必须先卸：连击只在
#     空手时触发，且持武器时 attack_type 走武器技能、prepare 槽被忽略；
#   · enable_practice 段的 /prepare force sword 已撞过拒绝分支，pmap 为空；
#   · 练功木人是 mirror_dummy 段 /ask 对练后的镜像（约八成的早期本道），
#     hp 低、无武器空手迎战，一两回合打不散，足够三段文本在同场出现。
#
# 并行说明（§8.6）：
#   · 请求/回执都是私发 tell（L2↔L9 一对一），只有本账号收得到；
#   · set_skill 的 sys 频道广播若推给全部在线（§3.5 实测项），其文案
#     「巫师一号设置…技能…为N级。」不含任何道的等待串，且 L9 只在
#     本道请求后触发一次——不会被并行道假匹配，也不会假匹配并行道；
#   · kill 的房间播报「突然发动袭击」含木人专属名，L4 在同房只等
#     本人版休息文本，互不相撞；战斗全程在 per-kill 战斗房实例内，
#     播报不外溢到共享房间。
#
# 时序：L9 登录后约 10 秒即进入 wait text 组合供能请求（宽 timeout 300s），
#   本段约百秒后到达服务点 → 请求方发完即进 wait（§3.3），回执毫秒级返回。
#
# 目标纪律（§8.5）：本段四次切目标（巫师/铁剑/技能激发不需要/木人），
#   每步显式 /t，段尾 /t 0 收干净，不给 skill_admin.sce 留隐式目标。

# ---------------------------------------------------------------------------
# 1) 请求 L9 供能：unarmed/hand 基础 + 两门高级（等级取值见 L9 服务段头注；
#    高级 10 级使 query_action 落在确定的第一档动作，断言才不含随机）
# ---------------------------------------------------------------------------
cmd /t 1
cmd /tell 组合供能请求，组合供能账号是 $acct
wait text 组合供能完毕 timeout=300000

# ---------------------------------------------------------------------------
# 2) 卸下铁剑：连击分支（Combatd::fight）要求 !weapon；
#    unwield 的本人版回执是「你将手中的铁剑插回剑鞘。」（unwield_msg）
# ---------------------------------------------------------------------------
cmd /t sample_sword_basic
clear
cmd /unwield
wait text 插回剑鞘 timeout=10000

# ---------------------------------------------------------------------------
# 3) 激发双空手槽（user 命令留本道自做——与 /enable force/sword 同一覆盖面）
# ---------------------------------------------------------------------------
cmd /t 0
clear
cmd /enable hand hand_adv
wait text Ok. timeout=10000
clear
cmd /enable unarmed unarmed_adv
wait text Ok. timeout=10000

# ---------------------------------------------------------------------------
# 4) 组合（两技能形；后加入者 hand_adv 的 valid_combine 放行）
# ---------------------------------------------------------------------------
clear
cmd /prepare hand_adv unarmed_adv
wait text 完成技能准备 timeout=10000

# ---------------------------------------------------------------------------
# 5) 打木人实证一次两招：每回合「常规击（v[0] 槽）+ 连击（v[1] 槽，带
#    「紧跟着」前缀）」——三段文本同场出现即双槽都被实战调用；
#    动作文本是攻击结果的一部分，命中/被躲都会输出，不赌命中。
#    木人散架 → die 钩子自毁 → 战斗房 end_fight → battle_end。
# ---------------------------------------------------------------------------
cmd /t mu ren
clear
cmd /kill
wait battle timeout=10000
wait text 紧跟着 timeout=60000
wait text 力劈华山 timeout=60000
wait text 连续快攻 timeout=60000
wait battle_end timeout=120000
cmd /t 0

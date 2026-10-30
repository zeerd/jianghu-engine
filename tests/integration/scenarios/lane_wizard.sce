# lane_wizard.sce — L9 道：巫师服务（为有特别战斗需求的道改定属性/技能）
#
# 账号机制：巫师一号（cc_id=1）随服务端启动自举（Sql.cpp ensure_wizard_accounts，
#   grant=100），不走注册流程——run-integration-tests.ps1 对本道 NoRegister，
#   vars 直写 acct=1 + pass（config [wizard] password，缺省 12345678）。
#   首登 cc_savedata 为空 → verify_user 新人分支 init_new_char 建档，
#   age/start_city 取 config（start_age=16 / start_room=青云山山脚）。
#
# 服务协议（§3.3）：需求道在服务点 `/t 1` + `tell` 请求 → 本道按固定顺序
#   `wait text` 收请求 → 执行服务段 → 私发回执；需求道 wait 回执或直接状态断言。
#   本道服务段之间禁止 clear（§8.6⑦）。
#
# 稀缺资源归属（§8.4③）：本道不采购、不占用任何共享世界资源；巫师命令只
#   改定目标账号的 dbase/技能，不动物品。

include common/login.sce

# 自验：巫师命令链通路（grant 100 → get_room_char/find_player → full）。
# /t 1 指向自己（巫师账号数字 ID 固定 1），full 顶满 hp/eff_hp/mp/eff_mp，
# 回执为房间版播报「只见巫师一号浑身金光乍现…」——用它确认命令被服务端执行。
cmd /t 1
cmd /full
wait text 金光乍现
assert attr hp >= 1

# ---------------------------------------------------------------------------
# 服务段（§3.3 协议）：需求道 /t 1 + tell 请求 → 本道按**固定 wait 顺序**
#   收请求 → 执行服务 → 私发回执。wait 顺序（≠到达顺序，靠缓冲匹配兜住）：
#   冲级(L5) → 山贼技能/内力(L8 两次) → 偷袭(L3) → 剑气(L2) → 组合(L2) → 激励(L2) → 练功(L10)。
#   先到的请求滞留缓冲（§8.6⑦：全程不 clear），L9 逐段消费——各道的
#   receipt 都是毫秒级；请求词与 capture 标记全服唯一（冲级/技能/内力/偷袭/
#   剑气/组合/激励/练功 供能账号是），多请求先后不保证时 capture 也不会抓错账号；
#   receipt timeout 均 300s——仅失败路径才真正等满。
#   服务段全程不 clear（§8.6⑦：单发私聊会在途，clear 会把请求/回执吞掉且永不重发）。
# ---------------------------------------------------------------------------
wait text 冲级供能请求 timeout=360000
capture 冲级供能账号是 as lvl_acct
cmd /t $lvl_acct
cmd /call set level 10
cmd /tell 冲级供能完毕
cmd /t 0

wait text 山贼技能供能请求 timeout=360000
capture 技能供能账号是 as raid_skill_acct
cmd /t $raid_skill_acct
cmd /set_skill force 10
cmd /set_skill force_adv 10
cmd /call set max_mp 4
cmd /tell 山贼技能供能完毕
cmd /t 0

# L8 的内力供能必须在其 /enable force 之后（enable 把 mp 清零）——L8 侧拆了
# 两次请求，本段固定排在技能供能之后、按到达顺序消费
wait text 山贼内力供能请求 timeout=360000
capture 内力供能账号是 as raid_mp_acct
cmd /t $raid_mp_acct
cmd /call set mp 4
cmd /tell 山贼内力供能完毕
cmd /t 0

wait text 偷袭供能请求 timeout=360000
capture 偷袭供能账号是 as sneak_acct
cmd /t $sneak_acct
# throwing 线：N4 拆分后 learn_throwing 迁往 L10，L3 的技能改由本道供能；
# enable 仍留需求道自做（用户命令）；mp 供四投预算（见 L3 sneak 段头注）
cmd /set_skill throwing 10
cmd /set_skill throw_adv 10
cmd /call set mp 400
cmd /tell 偷袭供能完毕
cmd /t 0

# L2 剑气供能（sword_qi 段）：do_qi 硬门槛 sword_adv≥60 + mp≥100——
#   本道学的 sword_adv 只有 11 级、mp 被 enable 清零，均供能直设；
#   L2 请求约 130s 到达，排在偷袭（L3 ~100s）与组合（L2 ~150s）之间
wait text 剑气供能请求 timeout=360000
capture 剑气供能账号是 as qi_acct
cmd /t $qi_acct
cmd /set_skill sword_adv 60
cmd /call set max_mp 150
cmd /call set mp 150
cmd /tell 剑气供能完毕
cmd /t 0

wait text 组合供能请求 timeout=300000
capture 组合供能账号是 as svc_acct
cmd /t $svc_acct
cmd /set_skill hand 50
cmd /set_skill unarmed 50
cmd /set_skill hand_adv 10
cmd /set_skill unarmed_adv 10
cmd /tell 组合供能完毕
cmd /t 0

# L2 激励供能（powerup 段）：do_powerup 的 mp≥100 硬门槛——打坐攒 mp 远超
#   E2E 时长预算，供能直设。L2 请求约 180s 到达，早于 L10 练功供能（220s），
#   排在组合与练功之间按序消费
wait text 激励供能请求 timeout=360000
capture 激励供能账号是 as powerup_acct
cmd /t $powerup_acct
cmd /call set max_mp 200
cmd /call set mp 200
cmd /tell 激励供能完毕
cmd /t 0

# L10 练功供能（N4 L3 拆分）：no_raise 段要巫师造大上限（本道无树箱经验果）。
# 排链尾——L10 约 220s 到达，晚于前四段服务；即使前序等待尚未结束，本请求
# 滞留缓冲不丢（全程无 clear）。
wait text 练功供能请求 timeout=360000
capture 练功供能账号是 as train_acct
cmd /t $train_acct
cmd /call set max_mp 100
cmd /tell 练功供能完毕
cmd /t 0

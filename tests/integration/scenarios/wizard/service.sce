# service.sce — 巫师服务链（固定顺序收请求 → 执行 → 私发回执）+ 链尾房间重整
#
# 业务：按固定 wait 顺序消费十路供能/查询请求——收徒资格(L5) → NPC 师徒家门
#   查询(L4) → 冲级(L5) → 毒术/内力(L4) → 山贼技能/内力(L8 两次) → 偷袭(L3) →
#   剑气(L2) → 组合(L2) → 激励(L2) → 练功(L10)。请求词与 capture 标记
#   全服唯一，先到者滞留缓冲、由后续 wait 按序消费；receipt timeout 均 300s——
#   仅失败路径才真正等满。
# 前两段是链首请求（L5 的 master_recruit_prep、L4 的 common/apprentice 之后立刻
#   发出，约 25~50s），按到达时刻排在最前：既让这两道尽早拿到回执，也不推迟
#   后面任何一段（顺序依据见 §2.6）。
#
# 判定：每段 = 请求到达（capture 账号）→ 巫师命令改定目标账号 → 私发回执；
#   需求道侧断回执或直接断状态。**本段全程不 clear**（§8.6⑦：单发私聊会在途，
#   clear 会把请求/回执吞掉且永不重发）——故上一片段 self_verify 也不 clear。
#
# 链尾 /update 重整脚下一间房（无目标即自身环境）。目标房选青云城住宅区：全轮此刻
#   无人停留、房内只有月老（角色不进清理面），是唯一没有地物归属的候选房；山脚三箱
#   （木箱/药箱/锁箱）、山顶树箱、演武场比试房各有其主，禁选。单次重整只给房内地物
#   打「待清」标记、二次才真清——全轮只 reset 这一次，标记不产生可观测变化；
#   房间未声明存盘键，重整前置的那一次读档直接落空，不会用旧档覆盖现房态。
#
# 放置：lane_wizard 的第三个 include（self_verify 之后，道尾）。
# 动线前提：本道此前不移动（服务跨房 /t 定向），故链尾才第一次换房。

# ---------------------------------------------------------------------------
# 服务段：需求道 /t 1 + tell 请求 → 本道按**固定 wait 顺序**
#   收请求 → 执行服务 → 私发回执。wait 顺序（≠到达顺序，靠缓冲匹配兜住）：
#   收徒资格(L5) → NPC 师徒家门查询(L4) → 冲级(L5) → 毒术/内力(L4) →
#   山贼技能/内力(L8 两次) → 偷袭(L3) → 剑气(L2) → 组合(L2) → 激励(L2) →
#   练功(L10)。
#   各道的 receipt 都是毫秒级；多请求先后不保证时 capture 也不会抓错账号。
# ---------------------------------------------------------------------------

# L5 玩家师父的收徒资格（master_recruit_prep 段）：/recruit 与 /apprentice 的
# 资格守卫读 dbase recruit_exp，新人恒 0（用户命令 /rexp 只在已有收徒记录时
# 才回填）→ 由本道直设 50。L6 在会合点等 L5，回执越早、会合越不空等。
wait text 收徒资格供能请求 timeout=360000
capture 收徒资格供能账号是 as rec_acct
cmd /t $rec_acct
cmd /call set recruit_exp 50
cmd /tell 收徒资格供能完毕
cmd /t 0

# L4 场景 C 的家门查询：NPC 师父收徒不写 family/user → 直读 dbase 断言。
# 回执带值（capture 抓数字回传），L4 断「family/user=0」。
wait text NPC师徒家门查询请求 timeout=360000
capture NPC师徒家门查询账号是 as fam_acct
cmd /t $fam_acct
cmd /call query family/user
# 回包先等到再抓（capture 不等网络回包，紧跟 cmd 会空抓）
wait text family/user= timeout=10000
capture family/user= as fam_user
cmd /tell NPC师徒家门查询完毕 family/user=$fam_user
cmd /t 0

wait text 冲级供能请求 timeout=360000
capture 冲级供能账号是 as lvl_acct
cmd /t $lvl_acct
cmd /call set level 10
cmd /tell 冲级供能完毕
cmd /t 0

# L4 涂毒供能（毒药/补药对照段）：poison_base.do_mo 的成功支要「毒术 + 手持
# 兵器 + 内力≥30」，样例世界没有毒术师父，毒术与内力由本道直设；毒术等级
# 同时是涂毒时效（do_mo 以等级为拍数登记 remove_effect）。
# 排在冲级之后、山贼技能之前：L4 约 60s 到达，早于 L8 的技能请求（~100s），
# 既不推迟后续任何一段，也不让 L4 干等
wait text 毒术供能请求 timeout=360000
capture 毒术供能账号是 as poison_acct
cmd /t $poison_acct
cmd /set_skill poison 10
cmd /call set mp 400
cmd /tell 毒术供能完毕
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
#   空手半边另供 hand_adv 10（do_qi 的掌法门槛，也是 /enable、/prepare 的前置）
#   L2 请求约 130s 到达，排在偷袭（L3 ~100s）与组合（L2 ~150s）之间
wait text 剑气供能请求 timeout=360000
capture 剑气供能账号是 as qi_acct
cmd /t $qi_acct
cmd /set_skill sword_adv 60
cmd /set_skill hand_adv 10
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

# --- 链尾自收口：房间重整（/update，无目标即重整脚下这间房） ------------------
cmd /go 青云城住宅区
wait room 青云城住宅区 timeout=15000
clear
cmd /update
wait text OK. timeout=10000

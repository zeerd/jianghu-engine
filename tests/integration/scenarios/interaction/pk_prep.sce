# pk_prep.sce — L5（PK 赢家账号）的 PK 前置：起步历练 → 买入青锋剑并装备 → 巫师供能冲 10 级
#
# **段落位置：必须排在 sleep（双人同睡）之后、fight 之前**（见 lane_interaction_a
# 头注）——历史版本用 /upgrade 冲级，其经 setup_char→setup_human_max_hp 按
# base=300+level×con×3 重算 max_hp 且对已定义的 hp 不补：10 级时 max=972 而
# hp 不变 → 血比跌破 50%，会被双人 sleep 的「心有余而力不足」守卫拒之门外。
# 现冲级改由巫师 `call set level 10` 直写 dbase（不触发 max_hp 重算，max 保持
# 405、hp 292 = 72% 血比），但段序仍锁定在 sleep 之后——若未来恢复 /upgrade
# 路径该顺序即为硬约束；下游 fight/pk 无任何 hp% 守卫（run10/11/12 以 35%
# 血打完全程已实证）。
#
# 设计（P4「要赢的账号 10 级、要输的账号 1 级，战斗很快结束」）：
#   · 准备只打在这一个账号上：全部步骤都是本账号自取——起步历练按
#     reward/exp 每号各领一次、冲级由 L9 巫师 `call set level 10` 供能
#     （N4 §3.3：/upgrade 的覆盖家是 L2 levelup_rest、L3 levelup_heal、
#     L4 vote，本道不再重复）。不请教师父、不动共享
#     回春丹/金创药库存、观战道（L7）完全不参与，对其余各道的耦合只剩
#     「山脚商人多卖出 1 柄青锋剑」；
#   · 等级差安全：结婚/夫妻组队（marriage/pair/divorce）整段在本段**之前**，
#     那时 A、B 同为 1 级差 0，couple_level_diff=10 恒满足；本段之后 A 才
#     升到 10 级开打（届时已离婚、无 couple 关系）。
#
# 制胜杠杆（按收益排序，实测与公式依据）：
#   ① 命中：skill_power 对无技能者取 combat_exp/2 —— 起步历练 5 万让攻击方
#      ap≈5 万 vs 徒手 dp=0 → 必中（徒手对殴 251s 的大头就是挥空）；
#   ② 基础伤害：青锋剑 apply/damage=30（wield 生效）+ str 加成 ≈ 48 点/拍；
#   ③ 等级：伤害公式 ×(100+level)/100 只读 dbase level——巫师
#      `call set level 10` 与 /upgrade×9 在伤害上等价（+10% 伤害），
#      max_hp 是否被重算不进该公式；
#   ④ 技能刻意不学：sword 每级仅 +skill/4 点伤害（8 级合计 +2），却要
#      师父/回春丹/气血轮转三重耦合——收益最小、面最大，弃用。
#
# 气血（sleep 在本段之前）：建档 eff/hp=342（注册按未写入的
#   level=0 计 300+42，age 加成 42）；登录重算 max=405（level=1）；同睡
#   wakeup2 扣 kee_cost=50（=50+5×(str-con)，str=con）→ **292 血**进本段，
#   本段零耗气 → 上阵 292 血。巫师直写 level 不触发 max_hp 重算——max 保持
#   405、hp 292 = 72% 血比，无 hp% 守卫的下游（fight/pk）照常开打
#   （run10/11/12 实测 35% 血打完全程）。
#   A 的闪避 dp 同样来自 5 万历练（≈2.5 万），徒手的 B 打不中 A。
#
# 结算预期：B（1 级、同睡后 292 气血）约 6 拍 → A 单挑 30s 内、观战道加入后更快。
#
# 并行说明：买剑/装备/冲级回执全是本人版（「你…」/私发）；起步历练每
#   账号一次，与各道并发互不干扰。

include ../common/room_look_inquiry.sce

# --- 买入并装备青锋剑（山脚杂货老板，商品 2 = carry_good 顺序）---
# 本段在 sleep 之后：sleep_a 结尾换房回山脚，wait room 与对象名录的 ADDOBJECT
# 可分批到达（room_look_inquiry 等的是「老李头」，同批与否取决于 TCP 分段）——
# 直接等本段要用的对象，/t 英文 id 才不落空
wait object 杂货老板
cmd /t huo ji
clear
cmd /buy 2
wait text 你从杂货老板那里买下了数量1的青锋剑 timeout=10000
wait item 青锋剑 timeout=10000

cmd /t sample_sword_adv
clear
cmd /wield
# 文本断言取本人版（「你「锵」…」）——房间版会与同轮其它装备动作互相
# 假匹配（§8.6②）；状态断言等 flush_my_equip 的 RTNITEM 分批推送到位
wait text 你「锵」的一声拔出一柄 timeout=10000
wait wield 青锋剑 右手 timeout=15000

# --- 冲到 10 级：L9 巫师供能（§3.3 N4）---
# /upgrade 的覆盖家是 L2 levelup_rest、L3 levelup_heal、L4 vote——本道不再
# 重复；巫师 call set level 10 直写 dbase，伤害公式 ×(100+level)/100 只读
# level，与 /upgrade×9 在伤害上等价。请求/回执私发（§8.6①），请求词与
# capture 标记全服唯一；发完即进 wait（§3.3）。attr 断言真实 level（§8.4①）
# ——call set 后 update_me 推 STATE。
cmd /t 1
cmd /tell 冲级供能请求，冲级供能账号是 $acct
wait text 冲级供能完毕 timeout=300000
wait attr level >= 10 timeout=10000
cmd /t 0

# pk_prep.sce — L5（PK 赢家账号）的 PK 前置：起步历练 → 买入青锋剑并装备 → 连升至 10 级
#
# 设计（P4「要赢的账号 10 级、要输的账号 1 级，战斗很快结束」）：
#   · 准备只打在这一个账号上：全部步骤都是本账号自取——起步历练按
#     reward/exp 每号各领一次、冲级是纯 /upgrade。不请教师父、不动共享
#     回春丹/金创药库存、观战道（L7）完全不参与，对其余各道的耦合只剩
#     「山脚商人多卖出 1 柄青锋剑」；
#   · 等级上限安全：赢家 10 级 vs 输家 1 级差 9，在 couple_level_diff=10
#     之内，结婚/夫妻组队段（P3）不受影响。
#
# 制胜杠杆（按收益排序，实测与公式依据）：
#   ① 命中：skill_power 对无技能者取 combat_exp/2 —— 起步历练 5 万让攻击方
#      ap≈5 万 vs 徒手 dp=0 → 必中（徒手对殴 251s 的大头就是挥空）；
#   ② 基础伤害：青锋剑 apply/damage=30（wield 生效）+ str 加成 ≈ 48 点/拍；
#   ③ 等级：伤害 ×(100+level)/100 —— /upgrade×9 到 10 级 = +10% 伤害，
#      level2skill(10)=39 同时为可选的后续技能深造留足上限；
#   ④ 技能刻意不学：sword 每级仅 +skill/4 点伤害（8 级合计 +2），却要
#      师父/回春丹/气血轮转三重耦合——收益最小、面最大，弃用。
#
# 气血：新角色建档 eff/hp=342（注册按未写入的 level=0 计 300+42），
#   本段零耗气，342/342 满状态上阵；A 的闪避 dp 同样来自 5 万历练
#   （≈2.5 万），徒手的 B 打不中 A，血量无需再抬。
#
# 结算预期：B（1 级、342 气血）约 7 拍 → A 单挑 30s 内、观战道加入后 ~20s。
#
# 并行说明：买剑/装备/冲级回执全是本人版（「你…」/私发）；起步历练每
#   账号一次，与各道并发互不干扰。

include ../common/room_look_inquiry.sce

# --- 买入并装备青锋剑（山脚杂货老板，商品 2 = carry_good 顺序）---
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

# --- 冲到 10 级（/upgrade 每次 +1 级；5 万起步历练足够——
#     level2_exp(10)=7500，valid_exp 全程为正）---
repeat 9
clear
cmd /upgrade
wait text 升级成功 timeout=10000
end
wait attr level >= 10 timeout=10000
cmd /t 0

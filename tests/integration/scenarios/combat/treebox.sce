# treebox.sce — 上山顶：等随机怪树刷出 → 和怪树聊天摘下经验果 → 服下（内力上限 +100）
#
# 覆盖：sample_guide.lua 的 do_refresh 随机 add_item 刷怪树（1/5 每 5 秒掷一次，
#   刷出后停止重排）、sample_tree.lua 的物件命令截获（对怪树 talk 即摘果，
#   含「果子已摘光」的私发回执）、sample_xp.lua 的经验果 do_use
#   （combat_exp +100000、max_mp +100、FlushMyInfo）。
#
# ⚠ 并行隔离：怪树全服只刷一棵、经验果只有一粒（random/tree 置位后不再重排，
#   怪树带 no_clean_up 不随房间 reset 消失），故摘果段只由 L3 一道执行——N4 拆分后
#   偷袭 mp 改由 L9 供能，经验果的 +100 上限不再是偷袭前置，但随机刷树 /
#   talk 截获 / do_use 的世界覆盖仍钉在本道（§3.3 原地不动）。
#
# 判定：
#   · 山顶场景描述私发；`wait object` / `wait item` 都是每客户端独立状态；
#   · 观察怪树断私发的描述文本，摘果断本人版房间播报 + 断言背包里真多了一粒
#     经验果，再断言本人版服用回执「你吃下一粒经验果」；
#   · 服下后 FlushMyInfo 会推送 max_mp，用状态断言直接验证上限确实 +100
#     （拆分后本道不再打坐，上限从 0 抬到 100）。

cmd /go 青云山半山腰
wait room 青云山半山腰
# 山脚无直达山顶的门（sample_mountain_foot 只 add_door 半山腰/茶棚/钱庄/住宅区），
# 必须经半山腰中转——旧链的 common/apprentice 恰好先到半山腰，N4 拆分后
# 本道不再拜师，故显式两跳
cmd /go 青云山山顶
wait room 青云山山顶

clear
cmd /look
wait text 山顶前方有一座山寨 timeout=10000

# 随机怪树：概率性刷新（脚本 1/5 每 5 秒重掷，刷出即停）。
# 超时是「刷树链断了」的死锁护栏，不是掷骰的统计上界——按 1/5 每 5 秒算，
# 420 秒约 84 次掷、失手概率约 1e-8，护栏落在分布内侧就会随机变红
wait object 一个奇怪的树 timeout=420000
cmd /t 0
cmd /t tree
cmd /look
wait text 枝头挂着一枚晶莹的果子 timeout=10000

# 摘果：物件截获定向 talk（先于内建 talk 命令），脚本把经验果 move 进背包
cmd /talk 摘果子
wait text 你伸手从怪树上摘下了经验果 timeout=10000
wait item 经验果 timeout=10000

# 果子只有一枚：再聊只收到「已摘光」私发（handle_action 的 picked 分支）
cmd /talk 再摘一个
wait text 枝头已经没有果子了 timeout=10000

cmd /t jing yan dan
clear
cmd /use
# 经验果的 do_use 会 start_busy(2)（sample_xp.lua），后续 sneak 的 cast 准入
# 拒 busy（CmdStandardCombat cast 入口）——必吃分支必产生 busy：回执可能被
# 同房广播挤到 busy 之后，先收起始 STATE（轮询容忍在途），再等回执与结束
wait busy timeout=5000
wait text 你吃下一粒经验果 timeout=10000
wait attr max_mp >= 100 timeout=10000
wait not_busy timeout=10000
cmd .info
cmd /t 0

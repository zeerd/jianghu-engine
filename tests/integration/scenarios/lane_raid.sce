# lane_raid.sce — L8 道：山贼试炼（N2 新增道，2026-10-31；2026-11-01 破障实测修订）
#
# 职责：闯入黑风寨与两名山贼开战——黑风寨副本的**第二入口窗口**、enforce/打坐
#   内力经济的实战组合、山贼战的 force hit_ob 三闸正常分支（Attack.cpp:364：
#   mp_factor、mp≥factor、force 已激发，L9 供能链保证三闸全开）。
#   原覆盖目标 `l_perform_action`（山贼 chat_msg_combat 随机出招）**已烧**（前轮
#   覆盖率轮）；`l_receive_wound`（force 反震分支）经 2026-11-01 破障实测判定
#   **结构性不可达、列入不测**（force.lua 的 mp_you ≤ 3mp 封顶使 damage<0 恒不
#   成立）。
#
# **降断言设计（2026-10-31 拍板；2026-11-01 依据更新）**：剧本**不反验反震/出招**，
#   只验「开战 → 战斗收束 → 战后信号」（稳定绿）。原四重障碍中的「拳拳 miss」
#   已被破障实测量化（我 sp 15500 vs 山贼 DP≈4.9万/PP≈9.8万；set_skill unarmed
#   300 起 P(命中)≥91%）——但破障对覆盖无新增收益（两目标一个已烧一个不测），
#   故不为破障改本道剧本。con30（Attrs=10 14 30 30 → max 1260）撑战前药与战斗
#   窗口；实测细节见 raid_prepare/raid_assault 头注。
#
# 为什么不打赢：山贼 sp≈6.5~14.6 万 vs 我方 dp 顶格 5.1 万——闪避/获胜数学上
#   不可能，败退或重伤是必然结局（两种战后落点都不影响信号发送）。
#
# 与 L3 的错峰契约：黑风寨是**全局单例副本**（design §2.4）。本道先开战
#   （N4 拆分后约 110s vs L3 约 225s），战事结束 wait「L3已到寨口」收到即
#   tell L3「黑风寨已清」；L3 在 `menu 闯入山寨` 前 wait 该词才进，且进前
#   补回春顶 hp 承穿行的 0~2 击（山贼仇恨/房态分析见 raid_assault 头注）。
#   本道若中途失败，L3 的 wait 超时连带失败——排查按 §9「疑似对端未到位」
#   先看本道日志。
#
# 稀缺资源归属（§8.4③ / §8.6⑤，L8 视角）：
#   · 山脚金创药 9 粒、回春丹 10 粒（**不买剑**：徒手走 unarmed 攻击链，剑伤
#     不进本道战斗设计）；本道约 221s（N4 拆分后实测）不落关键路径（L2 231s）；
#   · 黑风寨副本的**山贼战**归本道（L3 只路过大厅找小头目离开，不打山贼；
#     庭院 spawned 首刷由本道触发，两道以信号错峰、不同时间开战）；
#   · 不碰：山脚木箱/药箱（L1）、山顶树箱经验果与青蛇（L3）、排行榜（L1）、
#     蛇胆与传送符（L4）、L5/L6 的互动资源。
#
# peer 变量：PeerId=3 → 本道 vars 收 `peer_acct`（L3 账号 ID）供战后 tell；
#   只发不收（L3 端 wait 的是文本信号，不需要本道账号）。
#
# 注册天赋：Attrs="10 14 30 30"（register_clerk 校验：单项 10~30、总和 84）——
#   con30 是生存轴（max_hp 1260），dex30 顺带拉高 dp（仍不足以闪避，仅尽人事）。

include common/login.sce
include common/room_look_inquiry.sce
include provision/raid_supplies.sce
# 拜师必须在 force 链之前（learn 要求 is_apprentice_of）——apprentice 结尾停半山腰，
# 后续请教/打坐/出寨动线都从半山腰接续
include common/apprentice.sce
include raid/raid_prepare.sce
include raid/raid_assault.sce

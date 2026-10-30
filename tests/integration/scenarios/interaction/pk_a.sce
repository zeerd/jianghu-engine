# pk_a.sce — 玩家 PK 致死链（A 方：kill 开战 → 观战道加入 → B 死亡 → PK 结算 → 自己撤离）
#
# 覆盖（P4 核心段）：
#   CCmdStandard::kill 玩家对玩家**成功**路径：sample 世界 attack_gate 放宽后，
#     CChar::cmd_hit_ob 开出非 threshold 战斗房（begin_fight(0)）→ CUser::ready_pk
#   CReward 的 PK 段（此前因 C++ 75 级硬编码门在 E2E 不可达，P4 下放 hook 后打通）：
#     killer_reward 的 user×user 分支 → victim_lose → handle_pk_reward → pk_reward
#     （A 侧私发「你PK玩家…当前罪行…」即收敛点）
#   CFightRoom::do_flee：B 死亡结算后 A /halt 逃离（非 threshold、AP 已远超 50）
#   死亡链见 pk_b.sce（unconcious → die → revive(quiet)）。
#
# 为什么 B 先倒：战斗房首拍即 team A 出手（turn=1 → acting=team A），A 恒比 B
#   多打一拍；双方四维同为 register 定值 21、无技能无武器，单拍伤害同分布，
#   观战道随后并入 A 队再叠一份输出——B 的 hp 恒先穿底。
#
# 三方时序（§8.6④）：开战即跨房 tell 观战道（$spectator_acct）「第二轮开打」；
#   观战道 ~2s 内 /join 入队，而 B 死亡至少需要 20s 连续输出，加入窗口充足。
#   A 只等 B 的死亡结算私发——若观战道/对端（L6）任一停摆，`wait text 你PK玩家`
#   超时信息即指向「疑似观战道（L7）或对端（L6）未到位」，先看 L7/L6 日志（§9）。
#
# 开场 sleep 1500 的作用：比试房（iid 同为 fight_room）刚销毁，留时间让观战道
#   消化 REMOVEOBJECT——其后的 `/t fight_room` 才保证命中新战斗房而不是陈旧对象。
#
# 并行说明：战斗房房间版战报只到房内三人；结算的两行频道播报一行走 sys
#   （仅巫师可见）、一行走 rumor（由 B 侧 `茶棚杀死` 收敛，见 pk_b 头注），
#   A 不依赖任何频道行。B 的尸体随 end_fight 落
#   茶棚，120 拍腐烂播报也只发茶棚房间（届时各道均已收工，且腐烂断言钉在 L3 山顶）。

# 比试房已销毁：留 1.5s 让观战道先消化 REMOVEOBJECT
sleep 1500

cmd /t $peer_acct
clear

# kill 成功 → 非 threshold 战斗房（state 断言，不吃旧回执）
cmd /kill
wait battle timeout=20000

# 通知观战道加入（房间 ADDOBJECT 先于本 tell 抵达 L7，/t fight_room 必然可解析）
cmd /t $spectator_acct
cmd /tell 第二轮开打
cmd /t 0

# 等 B 死亡 → PK 结算。
# A 侧收敛点用**房版死亡播报**（`<B>死了。`，die() 首行、无过滤发到战斗房内
#   三方）：`你PK玩家` 私发只到**致命一击者**（A/C 随机，不能押）；
#   pk_reward 已执行的证明链放在 B 侧——handle_pk_reward 的 rumor 发在其后，
#   B 断到即链成立（见 pk_b 头注）。
# 致死节奏（赢家 10 级 + 青锋剑 + 5 万历练 vs 1 级徒手 B）：ap≈5 万必中、
#   单拍 ~48 点，B 342 气血 ≈ 7 拍 → A 单挑 30s 内、观战道加入后更快；
#   360s 窗口是兜底——徒手时代实测 251s 曾以 11s 之差让 240s 窗口先到点、
#   死讯发给已退出的 A 全部丢失（服务器日志 handle_pk_reward 晚于 A 断线）。
wait text ${peer}死了 timeout=360000

# PK 结束立刻脱离：非 threshold 房、AP 早已远超 50 → do_flee（本人版「你慌慌张张…」）
cmd /halt
wait text 慌慌张张地逃离 timeout=15000
wait battle_end timeout=30000
wait room 山脚茶棚 timeout=30000

cmd /t 0

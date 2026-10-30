# spectate.sce — L7 观战道：比试房观战（guard）→ 观战退出（halt）→ 战斗房加入（join）→ 随场散出
#
# 覆盖：
#   CCmdStandard::guard：比试房观战成功（no_guard 未设）→ 本人版「你加入观战」
#   CCmdStandard::halt 观战分支：非战斗 → 移出战斗房 + 本人版「你离开了…」
#   CCmdStandard::join（join_battle）：非 threshold 战斗房 can_join 通过 →
#     入队回执「<名>加入战队0」+ CUser::ready_pk；入队后留场输出直至自然散场
#
# 变量：peer/peer_acct = L5（run-integration-tests.ps1 按 L5 的 WitnessId 注入），
#   仅用于回信号定向；战斗房以英文 id /t（iid=fight_room，见 §8.6⑥）。
#
# 三方时序（与 A 的 fight_a/pk_a 串成一条链，A 是唯一协调者）：
#   wait 比试开打 → guard → halt 退出 → tell 回信号（A 收到才 /halt 收场比试）
#   wait 第二轮开打 → join → 留场输出至散场（A 是唯一协调者，不等第二轮回信号）
#   两次 `/t fight_room` 必须落在前一间战斗房销毁之后（A 侧 pk_a 开场 sleep 1500
#   + 比试收场链的自然时长保证），否则 /t 命中陈旧对象、join 落「未知的命令」。
# 任一 wait 超时即「疑似 A 侧（L5）未到位」，先看 L5 日志（§9）。
#
# 并行说明：本道整段待在山脚茶棚（会合房间选取理由见 rendezvous.sce 头注）；
#   guard/退出/入队的房间版只发到战斗房内（A/B 在场），自己的回执全是本人版
#   或私发，不与任何他道断言交叉（子串已 grep 核对）。

cmd /go 山脚茶棚
wait room 山脚茶棚 timeout=30000

# --- 第一轮：比试房观战（A 入比试房后跨房 tell 信号） ---
wait text 比试开打 timeout=300000
cmd /t fight_room
cmd /guard
wait text 你加入观战 timeout=10000

# 观战退出（halt 的观战分支：非战斗 → 移出战斗房）
cmd /halt
wait text 你离开了 timeout=10000

# 回信号：A 收到后才 /halt 收场比试
cmd /t $peer_acct
cmd /tell 比试观战完毕
cmd /t 0

# --- 第二轮：PK 战斗房加入（非 threshold → can_join 通过） ---
wait text 第二轮开打 timeout=300000
cmd /t fight_room
cmd /join
wait text 加入战队 timeout=10000

# 撤出与散场：C 不做主动撤离——留场参战（本道无武装，输出次要，赢家 A
#   才是主输出），B 死亡后 A 先 /halt 撤出（do_flee 覆盖点），房间随
#   「敌队无人、我方仍在」在 ~12s 内 end_fight，C 被请出时收 do_leave_team
#   的私发「此次战斗耗时…」+ 战斗态复位回茶棚。窗口 360s 是远超实测
#   （预期 ~30-40s）的兜底——徒手时代 251s 的死亡曾让 240s 窗口以 9s 翻车。
wait text 此次战斗耗时 timeout=360000
wait battle_end timeout=60000
wait room 山脚茶棚 timeout=60000

cmd /t 0

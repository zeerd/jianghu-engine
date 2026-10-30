# fight_a.sce — 切磋（fight → pending/fight → do_bishi 比试房）+ 跟随/halt 分支（A 方）
#
# 覆盖：
#   CCmdStandard::follow：跟随他人（房间版，双方各自渲染）与 `/t 自己` 取消（私发）
#   CCmdStandard::halt：非战斗且不忙 → notify_fail 拒绝（忙碌中断分支由 L3 打坐段覆盖）
#   CCmdStandard::fight：A 首次出招只落 pending/fight（双方私发挑战语）；
#     B 应战后 do_bishi → CFightRoom（threshold 比试房，两人被一并吸入）
#   CCmdStandard::kill 在比试房内的拒绝分支：目标已在战 →「不能在观战中暗中偷袭」
#   CCmdStandard::halt（threshold 分支）：「跳出战圈」→ end_fight 双方回茶棚
#
# 三方时序（§8.6 松同步）：A 入比试房后跨房 tell 观战道（$spectator_acct =
#   run-integration-tests.ps1 按 L5 的 WitnessId=7 注入）「比试开打」，观战道
#   进房观战→退出→tell 回信号，A 收到信号才 /halt 收场——观战窗口确定存在。
#   观战道超时未回信号即「疑似观战道（L7）未到位」，先看 L7 日志（§9）。
#
# 并行说明：挑战语/应战语都是私发（message_vision 的 tell 分支只发当事人）；
#   战报与「身形向后一跃」是房间版——比试房内只有 A/B/观战道，且其 iid=fight_room
#   只在茶棚出现（L1 的茶棚段在 ~30s 前已收工），无串扰面。
#
# 前置动线：trade 段收尾在青云山山脚（no_fight=1），本段先回会合房间茶棚
#   （无 no_fight 才能切磋/开打）；复用 rendezvous 的双向松同步，任一侧先到
#   都等对方在场，避免 `/fight` 落「你要和谁切磋？」。
#   （include 路径按本文件所在目录解析，故为同目录引用）

include rendezvous.sce

# --- 跟随：A 跟随 B（A 自身渲染「你决定开始跟随…」，B 渲染「<A>决定开始跟随你…」） ---
cmd /t $peer_acct
cmd /follow
wait text 决定开始跟随 timeout=10000

# --- 取消跟随：/t 自己 → follow 命中 who==me 分支，私发「取消跟随！」 ---
cmd /t $acct
cmd /follow
wait text 取消跟随 timeout=10000

# --- halt 错误分支：不在战斗且不忙（notify_fail 私发） ---
cmd /halt
wait text 你现在不忙 timeout=10000

# --- 切磋邀请：首次 /fight 只落 pending/fight，自己视角私发挑战语（含「领教」） ---
cmd /t $peer_acct
clear
cmd /fight
wait text 领教 timeout=10000

# B 应战 → do_bishi：双方进入比试房，A 先进入战斗态（state 条件，不吃旧回执）
wait battle timeout=20000

# --- kill 错误分支：目标 B 已在战（比试房 join_team 已置 fight/is_fighting） ---
#     目标即 B，沿用上面的 /t，无需重设
cmd /kill
wait text 不能在观战中暗中偷袭 timeout=10000

# --- 观战联动：通知 L7 进比试房观战，等其退出后的回信号 ---
cmd /t $spectator_acct
cmd /tell 比试开打
cmd /t 0
wait text 比试观战完毕 timeout=60000

# --- 收场：threshold 分支「跳出战圈」→ end_fight → 双方回茶棚 ---
cmd /halt
wait text 跳出战圈 timeout=10000
wait battle_end timeout=20000
wait room 山脚茶棚 timeout=20000

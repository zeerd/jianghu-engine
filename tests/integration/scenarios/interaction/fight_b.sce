# fight_b.sce — 切磋（B 方：应战 → do_bishi → 等 A 收场）
#
# 与 fight_a.sce 配对并行。时序：
#   A 先 /fight 落 pending/fight → B 收到**私发**挑战语（message_vision 的
#   tell 分支只发当事人）→ B 回 /fight：命中 A 的 pending → do_bishi 两人入房。
#   A 在观战道退出后才 /halt（跳出战圈）收场，故 B 的等待取宽 timeout——
#   若 A 侧（含观战联动）停摆，本步超时即「疑似 A 侧未到位」，先看 L5 日志（§9）。
#
# 并行说明：应战语私发；「跳出战圈」是比试房房间版，房内只有 A/B/观战道，
#   断言子串不与任何他道重合（已 grep 核对）。
#
# 前置动线：trade 段收尾在青云山山脚（no_fight=1），本段先回会合房间茶棚——
#   与 A 侧同用 rendezvous 双向松同步（A 挑战语虽是私发跨房可达，但 do_bishi
#   的 get_room_char 要求同房，两边都到齐才开打）。
#   （include 路径按本文件所在目录解析，故为同目录引用）

include rendezvous.sce

cmd /t $peer_acct

# A 的切磋邀请（私发挑战语）
wait text 领教 timeout=60000

# 应战：第二次 fight 命中 A 的 pending/fight → do_bishi（比试房，threshold）
cmd /fight
wait battle timeout=20000

# 等 A（观战道退出后）跳出战圈收场 → 双方回茶棚
wait text 跳出战圈 timeout=90000
wait battle_end timeout=20000
wait room 山脚茶棚 timeout=20000

cmd /t 0

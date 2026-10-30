# sleep_a.sce — wakeup2 双人同睡（A 侧：发起方，男性账号）
#
# 覆盖（P1 遗留收口批次，CmdStandardLife 域）：CmdStandardLife.cpp Sleep
#   双人分支 + CCmdStandard::wakeup2 的**两个性别分支**（本侧钉男版）：
#   · 前置守卫链：目标同房间（get_room_char）+ 异性（注册性别参数化，L6=女性）
#     + sleep_room=1（只有青云山半山腰，sample_mountain_mid.lua）+ hp≥50% +
#     无 apply/index（本段在 pk_prep 之前，A 尚未买剑装备，天然无 apply 项）；
#   · hp 守卫的顺序前提：pk_prep（历史版本用 /upgrade 抬 max_hp 到 972
#     而 hp 不补 → 37% < 50% 会被「心有余而力不足」拒；现冲级改巫师
#     `call set level` 直写 dbase 不再触发重算，但段序仍锁定在本段之后
#     以保留对 /upgrade 路径的回退安全）排在本段**之后**——睡时 A 还是
#     1 级（max=405、hp=342 = 84%）。段落顺序见 lane_interaction_a.sce；
#   · 握手两段：第一声 /sleep 只设 pending/sleep 并私发邀请（本侧收
#     「对方正在考虑中」），对端第二声才落真睡——**必须等对端邀请先到**，
#     否则对端会落进邀请分支变成双向邀请、无人触发真睡（双方都醒不来）；
#   · 真睡触发时双方各收一条性别私发（男「你搂着…温软的身体，不由得心醉神迷」/
#     女「你躺在…的怀里…」——与触发顺序无关，性别决定收哪条）；
#   · wakeup2 按 call_out(random(45-con)+10 拍) 自然醒（con=21 → 10~33 秒），
#     各自独立随机、可能不同时醒——醒后分头回山脚，pk_prep/fight 的后续段
#     在山脚接续，无需等对方。
#
# 睡下时引擎对双方执行 unwield 0 all——本段 A 尚未装备（pk_prep 在 sleep 后），
#   无可卸之物；剑与冲级全部由紧随其后的 pk_prep 完成，本段不管装备。
#
# 断言纪律：全部为私发 tell（§8.6①）——半山腰是 L2/L3/L4 动线（师父/打坐/
#   拜师都在此），房间版（一笑/钻入被中/醒了过来）一律不断言；对端只需
#   $peer 文本断言（§8.6② 不适用，私发天然只到本人）。
# clear 位置（§8.6⑦）：只在片段开头——此刻 trade 握手已收口（复核好了已消费）、
#   对端不可能有在途信号；进入等待链后再无 clear。
#
# 前置动线：trade 段收尾在青云山山脚；半山腰只有山脚/山顶两道门，醒后须
#   先回山脚，后续 pk_prep（山脚）与 fight_a 的 rendezvous 才能接续。

clear
cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=30000
wait object $peer timeout=180000

# --- 第一声：发起双人睡（只落 pending + 私发邀请，不真睡） ------------------
cmd /t $peer_acct
cmd /sleep
wait text 对方正在考虑中 timeout=10000

# 对端第二声触发真睡 → 男版私发（含 $peer 名的温软身体句）
wait text 你搂着 timeout=10000

# --- 自然醒：random(45-con)+10 拍（con=21 → 10~33 秒），窗口给 60s ---------
# 男版私发（wakeup2 男性分支：「你醒了过来，似乎还沉浸在幸福之中。」）
wait text 你醒了过来 timeout=60000

# 回山脚（半山腰只有山脚/山顶两道门，会合房间须先落脚山脚）
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000


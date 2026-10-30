# pk_b.sce — 玩家 PK 致死链（B 方：被击杀 → 昏迷 → 死亡结算 → 复活落点）
#
# 覆盖（死亡→复活链，P1 遗留项在此收口）：
#   CChar::unconcious：战斗中 hp<0 而 eff_hp 未穿底（伤口按 ~25% 拍率削 eff_hp，
#     hp 每拍都掉，故先昏迷）→ 私发「你的眼前一黑…」
#   CChar::die：昏迷后下一拍 hp<0 且非存活 → die()；user 分支先 revive(quiet=1)
#     解 block，再清队/存档、hp=1、eff_hp=max_hp/2、迁 start_city（青云山山脚）
#   CChar::revive(quiet=1)：die() 内的静默复活（无文案，断言取状态而非文本）
#
# 判定纪律（§8.6①）——B 侧全部取私发/本人状态：
#   1. `wait battle`（state）：A 的 kill 把 B 拉进战斗房
#   2. `wait text 你的眼前一黑`：unconcious 在置 block_msg/all **之前**私发（tell 分支）
#   3. `wait text 此次战斗耗时`：die() 迁出战斗房触发 on_leave → do_leave_team →
#      tell_object（TELLOBJECT 不被 block 拦截）——F1 比试结束时 B 也收到过同款，
#      所以本片段开头必须 `clear` 清掉旧回执，否则这步会假匹配
#   4. `wait battle_end`（state）+ `wait attr hp <= 1`（die 后 flush 的定值）
#      + `wait room 青云山山脚`（start_city = config 的 start_room）
#   「<B>死了。」房间播报发生在 block 期间、会被拦，B/A 双方都不依赖它；
#   昏迷期 NOTIFY 房间战报同样被拦，故不作战报断言。
#
# 死亡结算三连，外加 PK 结算证明：
#   0. `wait text 茶棚杀死`：die() 内 killer_reward 的执行序是
#      （被…杀了，损失…行，sys 频道、仅巫师可见，E2E 收不到）→
#      pk_reward（私发+sys）→ handle_pk_reward 末尾的 **rumor** 行
#      「百姓…5在山脚茶棚杀死百姓…6，得经验0点。」——**断到 rumor 即证
#      pk_reward 已执行完**（执行序保证），且 rumor 频道全员可达、
#      「茶棚杀死」子串全服唯一（A/C 超时退出的历史翻车让它发得晚也无妨，
#      现在死亡 35s 内秒到）；与致命一击落在 A 还是 C 无关。
#   1. `wait battle_end`（state）+ `wait attr hp <= 1`（die 后 flush 的定值）
#      + `wait room 青云山山脚`（start_city = config 的 start_room）
#   「<B>死了。」房间播报发生在 block 期间、会被拦，B/A 双方都不依赖它；
#   昏迷期 NOTIFY 房间战报同样被拦，故不作战报断言。
#
# 结算各步在死亡后秒级到达（rumor 随 killer_reward、耗时随迁出），窗口 60s；
#   徒手时代 30s 窗口曾贴边，保留 60s 兜底。
#
# 并行说明：无任何房间/频道播报型断言对他人开放——`茶棚杀死`（rumor 行）
#   子串已 grep 全部道无同款，且只有本场用户 PK 会产生；A 侧的房版死亡
#   断言见 pk_a.sce。

# clear 是本片段的正确性前提：清掉 F1 比试留下的「此次战斗耗时」等旧回执
cmd /t 0
clear

# A 的 kill（同房间）→ 战斗房
wait battle timeout=60000

# 昏迷：hp<0 而 eff_hp 未穿底（unconcious 的置位前私发）。
# 赢家 10 级武装后预计 kill+~25s 昏迷、其后一拍死亡（360s 窗口为兜底——
#   徒手时代 240s 曾以 ~10s 余量贴边，同批 A/C 正是 240s 翻的车，见 pk_a 头注）
wait text 你的眼前一黑 timeout=360000

# PK 结算 rumor（顺序链证明，见头注 0）→ 迁出战斗房的清队私发 + 战斗态复位
#   + hp 定值 + start_city 落点；死亡后各项在 ~20s 内到达，窗口 60s
wait text 茶棚杀死 timeout=60000
wait text 此次战斗耗时 timeout=60000
wait battle_end timeout=60000
wait attr hp <= 1 timeout=60000
wait room 青云山山脚 timeout=60000

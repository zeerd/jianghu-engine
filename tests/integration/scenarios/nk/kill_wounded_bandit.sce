# kill_wounded_bandit.sce — 半山腰放倒重伤山贼：NK经验入账
#
# 覆盖（CChar::die 的击杀结算链）：
#   announce(dead) → killer_reward → nk_gain 折算 → handle_npc_kill_reward
#   封顶入账 → make_corpse；hooks.kill_bonus 走**缺席支**（重伤山贼不配
#   nk_bonus → 脚本返回 nil → 按 C++ 算出的值入账）。
#
# 数值推导（本道是全新账号，combat_exp=0 → dx = cx×4/9 = 0）：
#   受害者历练 3 → nk_gain 档位表最低档 → nkgain = 35
#   times = (3 − 0)/(0 + 1) = 3 → 钳到 2 → 奖励 = 35 × 2 = 70
#   击杀者 1 级封顶 level×4+100 = 104 > 70 → 不封顶，入账 70
#   本道全程不产生历练（攻/防值同为 1，无一支「攻<防」的经验分支成立），
#   dx 恒为 0；受害者挨打涨的那 1 点历练只让 times 更深地钳在 2 上。
#
# 判定（§8.4①，私发与本人状态优先）：
#   · 「获得70点NK经验」是击杀奖励的私发回执——精确值，全服只有本道收得到；
#   · attr combat_exp >= 70 是入账的状态证明（取下限：战斗收束另有零星经验）；
#   · 尸体 wait object 钉标准死亡链真的走到 make_corpse。
# 动线：山脚（登录落点）→ 半山腰一跳即到；本道收尾停在半山腰，不参与后续动线。

cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=10000
wait object 重伤山贼 timeout=10000
cmd /t zhong shang shan zei
clear
# /kill 记仇开战：两秒一拍、命中与负伤各掷一次——负伤把 eff_hp 打穿即死。
# 期望约 12 击（单击命中 50% × 招架 50% × 负伤 35.7% × 无甲 92% ≈ 8.2%），
# 按 4s/击 约 50s；240s 是「连掷不中」的死锁护栏，不是时长上界
cmd /kill
wait text 获得70点NK经验 timeout=240000
# 尸体要等战斗房收场、角色移回半山腰才落进房间
wait object 重伤山贼的尸体 timeout=20000
wait attr combat_exp >= 70 timeout=10000
cmd /t 0

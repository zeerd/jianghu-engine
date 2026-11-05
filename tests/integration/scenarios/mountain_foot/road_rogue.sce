# road_rogue.sce — 山脚林间空地：切磋与记仇的分野 → 对话记账 → 贿赂销账 → 打到死
#
# 覆盖（引擎侧 CmdStandardCombat.cpp 两条进战斗的正常路径 + 脚本侧配合）：
#   /fight 切磋 → CNpc::accept_fight → do_bishi 比试房，**全程不碰杀意名单**
#   /kill  记仇 → cmd_hit_ob + 双向 set_killer（源码注释即「记仇的」）
#   sample_road_rogue：
#     do_talk「挑衅」按强人自己那份追杀名单分岔（duanjia_sword.lua 的
#       target:is_killing(me) 同向）——第一次 set_killer 记账、名单已在则报出真支。
#       **set_killer 不开打**，它只把人写进名单（下次进房由 CChar::init 的 hatred
#       分支自动开战），所以同一句可以反复问；
#     do_talk「讨教」→ fight_ob 还手但不记账——这一句才真正开战；
#     accept_object 收受贿赂 → remove_all_killer 双方销账；
#     die 接管死亡：killer_reward（私发击杀奖励）→ MakeCorpse（房间里的尸体）→ 自毁
#   data/hooks.fight 的棋子补击（combat_attack_weapon + damge_to_string 私发）
#
# 顺序约束（为什么话全说在开战之前）：
#   · 战斗中 talk 被引擎挡死（CmdUserSocial.cpp 的 is_fighting 分支），所以
#     「挑衅 / 讨教 / 贿赂」三样都排在开战之前；开战由最后一句「讨教」触发，
#     之后不再需要说话，一路打到死，脚本因此没有任何收场脚手架；
#   · 贿赂同理在开战之前——非战斗态下 give 不走 30 AP 那道门。
#
# 判定（§8.4①，私发与房间状态优先）：
#   · **「想杀死你」的有无就是 kill/fight 的分野本身**——它由 set_killer 私发，
#     全服只有本道收得到；切磋那场与 fight_ob 开打那场都用 notext 盯住不出现；
#     但 notext 之前必须 clear，否则前面挑衅留下的那句会把它顶红；
#   · 「记下了 / 新账旧账 / 杀意已散 / 无仇无怨」是名单的真假支与销账回执，都私发；
#   · **销账的状态证明**：贿赂之后再问同一句挑衅，翻回「记下了」——没销账就还是
#     「新账旧账一起算」；
#   · 「扬起棋子」断 combat_attack_weapon（暗器一手即止，第一手就在这几拍里）；
#     「这一击」断 damge_to_string（打在他身上的每一击都私发出手方）；
#   · 尸体用 wait object 钉房间状态；NK 经验是击杀奖励的私发回执。
# 动线：进出一次空地，起终点都在山脚——下一段 drink_and_faint 全程待在
#   山脚不挪窝，同样要求本段收尾时仍在山脚。进房不自动开战（脚本没有 init 宣战），
#   故可以先 wait room 再定向，没有「切房推送抢跑」的竞态。
#
# 卸力（教材 parry_adv.lua，前置见 parry_xieli_prep.sce）：本段**顺带**承担
#   卸力的起势 / 持续 / 收势三步——本道唯一归己的战斗对手就是强人，而起势只能
#   在战斗中。落点选在「讨教开打、棋子补击断言已过」之后：棋子是一次性消耗暗器
#   （throw_weapon 扣 amount、剩 1 件即销毁），此刻起势再多打几拍也不会再动它的
#   资源（§8.4③ 稀缺资源唯一归属）。收势断在打到死的散场之后
#   （end_fight → do_leave_team → evaluate_call_out_combat），回执私发到本人。

# --- 场1：/fight 切磋，不进杀意名单 -------------------------------------------
cmd /go 山脚林间空地
wait room 山脚林间空地 timeout=10000
wait object 山道强人 timeout=10000
cmd /t shan dao qiang ren
clear
cmd /fight
wait text 领教 timeout=10000
wait battle timeout=15000
# 比试房全程没有记仇私发——切磋不碰名单（notext 到期无出现才通过）
wait notext 想杀死你 timeout=3000
# 阈值比试房的 halt 是「跳出战圈」，零门槛收场，双方回到空地
cmd /halt
wait text 跳出战圈 timeout=10000
wait battle_end timeout=15000
wait room 山脚林间空地 timeout=10000
wait object 山道强人 timeout=10000

# --- 出言不逊：第一次记账（set_killer 写名单，此刻 is_killing 为假）----------
clear
cmd /ask 挑衅
wait text 记下了 timeout=10000

# --- 同一句再来：名单已在（is_killing 真支）-----------------------------------
cmd /ask 挑衅
wait text 新账旧账 timeout=10000

# --- 贿赂销账：钱到气消（remove_all_killer）——还没开战，give 不吃 AP 门 -------
clear
cmd /give 金币 100
wait text 杀意已散 timeout=10000

# --- 销账的状态证明：同一句挑衅翻回假支 ---------------------------------------
cmd /ask 挑衅
wait text 记下了 timeout=10000

# --- 客气请教：fight_ob 还手但不记账，这一句才真正开打 -------------------------
# 必须 clear：上一句挑衅刚留了「想杀死你」在缓冲里，不清会把下面的 notext 顶红
clear
cmd /ask 讨教
wait text 无仇无怨 timeout=10000
wait battle timeout=15000
# 开打这一刻没有记仇私发——fight_ob 不碰名单（缓冲已清，这三秒里若记了账必露头）
wait notext 想杀死你 timeout=3000
# 补击的招式文案：combat_attack_weapon 的断言（暗器一手即止，第一手就在这几拍）
wait text 扬起棋子 timeout=15000

# --- 卸力起势（教材 parry_adv.lua，前置见 parry_xieli_prep.sce）----------------
# 棋子补击已断言过（一次性资源已耗），此后起势多打几拍不碰本段任何资源；
# 施法 AP 入队起已攒过若干拍，且供能把 parry_adv 顶到师父级 → 费夹到 1 点
wait not_busy timeout=10000
wait attr mp >= 40 timeout=10000
cmd /cast parry xieli
wait text 沉肩坠肘 timeout=10000

# 持续：状态在身的再施被拒——有这一条才区分得出「状态挂住了」与「起势即散」。
#   法术成功后引擎挂 1 拍 no_cast 冷却（心跳拍驱动、无状态可 wait，
#   §8.1 心跳计时的合法 sleep 用途），睡两拍再试
sleep 2000
wait not_busy timeout=10000
cmd /cast parry xieli
wait text 已经运起卸力 timeout=10000

# --- /kill 记仇：引擎那条路（双向 set_killer）---------------------------------
cmd /kill
wait text 想杀死你 timeout=10000
# 打在他身上的每一击都把实打伤害格式化私发给出手方（hooks.calculate_damage）
wait text 这一击 timeout=15000

# --- 打到死：击杀奖励 → 尸体 --------------------------------------------------
# 45s 是死锁护栏不是时长上界——打多久取决于伤害随机与出手节奏，护栏要站在分布之外
wait text NK经验 timeout=45000
wait battle_end timeout=45000
# 卸力收势：散场这一刻才触发 call_out_combat 的 delay=0 哨兵（心跳不走表）
wait text 卸力架势散去 timeout=10000
wait room 山脚林间空地 timeout=10000
wait object 尸体 timeout=10000

# 收口：回到山脚，下一段从这里起步（战斗刚收，先等真实不忙再走）
wait not_busy timeout=10000
cmd /go 青云山山脚
wait room 青云山山脚 timeout=10000
cmd /t 0

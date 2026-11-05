# sword_qi.sce — 剑气三连：武器招 + 空手招的组合绝招
#
# 覆盖（do_qi 三连击的确定性断言）：
#   l_combat_attack（Combatd.cpp:86）——do_qi 第一、三击（武器半边，
#     按手中铁剑的 skill_type=sword 取 sword_adv 出招表）
#   l_combat_attack_unarmed（Combatd.cpp:100）——do_qi 第二击（空手半边，
#     按 query_skill_mapped("hand") 取 hand_adv 出招表）
#   供给源 = L9 剑气供能（set_skill sword_adv 越 60 门槛 + set_skill
#     hand_adv 供空手半边 + call set mp 越 100 门槛）。
#
# 为什么「文本即证据」：两类动作文本出自两个不同技能的出招表——
#   剑招文本（白虹贯日 / 无边落木 / 平沙落雁）只有 sword_adv 会吐，
#   掌招文本（力劈华山）只有 hand_adv 会吐（10 级只落第一档，确定值）；
#   再加上三段 message_vision 的段标把两次剑招夹住，同场同时出现即证
#   两半都真打了。动作文本是攻击结果的一部分（命中/被躲都会输出，不赌命中）。
#
# 对手：半山腰铁人桩（sample_iron_dummy.lua，2000 血耐打、素人战力）——
#   绝招的教材喂招对象，三击打不死它，与木人（prepare_combine 的短平快
#   对手）分工明确。
#
# 前置（本道既有）：learn_sword 学会 sword_adv 并已持铁剑（/cast sword 的
#   引擎入口查「使用的武器不对」——持剑是前置）、enable_practice 已
#   /enable sword sword_adv（「请先准备技能」前置）。
# 目标纪律（§8.5）：/t tie ren zhuang 定向铁人桩，段尾 /t 0 收干净。
# 段尾两步（顺序不可倒）：
#   · /prepare none —— do_qi 的空手半边要 prepare，借完必须把 pmap 还成空，
#     否则 prepare_combine 的两技能 /prepare 会撞「你已准备有一种技能了」；
#   · wait not_busy —— do_qi 收尾的 start_busy（sword_adv.lua）由翻转 STATE
#     推送收敛，等到真实不忙才放行，否则下一段的 /unwield 会被「你正忙着。」拒掉。

cmd /t 1
cmd /tell 剑气供能请求，剑气供能账号是 $acct
wait text 剑气供能完毕 timeout=300000
wait attr mp >= 100 timeout=10000

cmd /t tie ren zhuang
wait object 铁人桩 timeout=10000

# ---------------------------------------------------------------------------
# 负支一：空手半边没激发（未 enable）→ do_qi 撞「没激发」门，整招不发
#   此时剑、内力、两门技能等级都已到位，唯一缺的就是 enable+prepare
# ---------------------------------------------------------------------------
clear
cmd /cast sword qi
wait text 你没有激发高级掌法 timeout=10000

# ---------------------------------------------------------------------------
# 负支二：激发了但没上槽（未 prepare）→ 撞「没准备好」门，同一扇门的另一半
# ---------------------------------------------------------------------------
clear
cmd /enable hand hand_adv
wait text Ok. timeout=10000
clear
cmd /cast sword qi
wait text 你没有准备好高级掌法 timeout=10000

# ---------------------------------------------------------------------------
# 正支：enable + prepare 齐了才放，三击同场
# ---------------------------------------------------------------------------
clear
cmd /prepare hand_adv
wait text 完成技能准备 timeout=10000

clear
cmd /cast sword qi
wait text 第一剑疾刺而出 timeout=10000
wait text 白虹贯日|无边落木|平沙落雁 timeout=10000
wait text 弃剑换掌 timeout=10000
wait text 力劈华山 timeout=10000
wait text 弃掌换剑 timeout=10000
wait attr mp <= 50 timeout=10000

# ---------------------------------------------------------------------------
# 段尾：还 pmap + 等 busy 走完（见头注）
# ---------------------------------------------------------------------------
clear
cmd /prepare none
wait text 取消全部技能准备 timeout=10000
wait not_busy timeout=10000
cmd /t 0

# iron_dummy_retaliate.sce — 半山腰：铁人桩的回合还手（attack 钩子 → base_attack）
#
# 覆盖：
#   l_base_attack（CharCombat.cpp 的 base_attack 绑定）——lua-sample/npc/
#   sample_iron_dummy.lua 的 attack 钩子把出手回合交回引擎基类 CChar::attack。
#   钩子只在战斗房里**轮到己方**的回合派发（CFightRoom::run_team_turn 对存活且
#   不忙的队员逐拍调 attack，两队交替、一回合一击）——不是挨打触发的额外出手
#   （挨打侧只有全局 hooks.receive 通告，没有每-NPC 的受击钩子）。所以必须真开
#   一战：/cast 只做单方结算、不开战（sword_qi 段即如此），钩子根本派发不到。
#   顺带：CCmdStandard::kill 的 NPC 分支（cmd_hit_ob 把双方搬进 per-kill 战斗
#   房）与 halt 的非 threshold 逃战分支（AP 门槛 → do_flee 回原房）。
#
# 判定（双证据，缺一面就不算演示完整）：
#   · 钩子面：「铁人桩身上的机簧咯吱一响」只出自 attack 钩子自己的表现——默认
#     回落（脚本没有 attack 时走 T::attack）不会发这声，它在场即证钩子被派发；
#   · 交回面：铁人桩出手回合的动作文本取 CChar::query_action 的人类动作表（五招
#     随机，不配技能的素人也走这张表），base_attack 交回后真打出来才有——断言写
#     五招并联串、不赌出哪招，动作文本命中/被躲都会输出，也不赌命中。串都以
#     「铁人桩」起头（§8.6②：含出手方名），战斗房内只有本道与铁人桩、它也只被
#     本道拉进战斗，别道与本道此前的 cast 文本都串不进本断言；
#   · wait battle / battle_end 是房间 F 旗（状态，design §7.1），wait room 钉住
#     开战离房与战罢回房两端。
#
# 节奏（等拍不赌秒）：/halt 要 50 AP，AP 逐拍按 dex 结算、首拍翻倍——本世界
#   register 定值 dex=21：起手 10、首拍 +20、此后每拍 +10，第 3 拍起即过门槛。
#   两次等待各钉一拍（第 2 拍交回面、第 4 拍钩子面），钩子表现由 advance_ap 之后
#   的出手先发，见到它即 AP=60，紧接 /halt 必过门槛；同一文本连等两次必须中间
#   clear（wait 扫的是累积缓冲，不 clear 第二次立刻吃旧回执）。
#
# 并行与道位（§8.4③）：铁人桩是本道的喂招对象（sword_qi 同房同目标），别道不
#   打它，故本段挂本道而不另开道；半山腰同房的 L4 只等本人版文本，kill 的房间
#   播报含铁人桩专属名，不进别道断言。
#
# 顺序：排在 prepare_combine / powerup 之后、skill_admin 之前——铁人桩打不死，
#   本段的 /kill 会留下一直有效的杀意名单，玩家此后每次进半山腰都由 CChar::init
#   的 hatred 分支重排一颗 5~10 秒后引爆的自动开战；本段是道上最后一处 /kill，
#   之后只剩 skill_admin 约一秒，雷落下时道已收口。本段只开战收战，不换装备
#   不动技能（断言全取铁人桩一侧的文本，与手上有没有剑无关）。

cmd /t 0
wait not_busy timeout=10000
wait room 青云山半山腰 timeout=10000
wait object 铁人桩 timeout=10000

cmd /t tie ren zhuang
clear
cmd /kill
wait battle timeout=10000

# 第 1 次出手（第 2 拍）：base_attack 交回引擎的攻击真打出来了（五招并联）
wait text 铁人桩挥拳攻击|铁人桩提起拳头|铁人桩往|铁人桩对准 timeout=15000

# 清掉旧回执，等第 2 次出手的钩子自表现（第 4 拍）：机簧咯吱只出自 attack 钩子，
# 且它先于本拍动作、由 advance_ap 之后发出——见到即 AP=60 已过 /halt 的 50 门槛
clear
wait text 铁人桩身上的机簧咯吱 timeout=15000

# 逃战收口：非 threshold 战走 do_flee——不掉胜负、不打死铁人桩，人回原房
cmd /halt
wait text 慌慌张张地逃离 timeout=10000
wait battle_end timeout=15000
wait room 青云山半山腰 timeout=10000
cmd /t 0

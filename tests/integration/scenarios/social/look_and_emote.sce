# look_and_emote.sce — 观察人物两条路 → 说话反应触发作揖
#
# 覆盖：
#   CChar::do_look 的外貌、武功评估与 eff_status_msg（杂货老板，无 do_look 脚本）
#   sample_guide.lua 的 do_look 钩子（老李头定义后完全接管观察、返回 1 不弹菜单）
#     与 eff_status_msg 全局绑定
#   relay_say → do_emote：老李头对定向说话作答回礼（见下方分节说明）
#
# 判定：
#   · 「打量了你一番」由 say(msg, me) 私发，天然本人专属；
#   · 「武功看起来」只有本道会去定向观察杂货老板（其余各道只与他买卖/询问）；
#   · 作揖由 bow 表情的 target 版送达玩家：「老李头对着你躬身作了个揖。」，
#     是 tell_object 私发；且只有本道发 /say 触发它。
#
# 前置：本道已由 social/poison_and_cure.sce 的传送符回到青云山山脚。

cmd /t 杂货老板
clear
cmd /look
wait text 武功看起来 timeout=10000
sleep 500

cmd /t 老李头
clear
cmd /look
wait text 打量了你一番 timeout=10000
sleep 500

# 老李头对「本道的定向说话」作答回礼——这是 lua-sample/npc/sample_guide.lua
# 提供的 relay_say 钩子（脚本接管「玩家对我说话」），由它调用引擎的 do_emote
# 绑定摆出 bow 表情；不是引擎自带行为，换一个没有该钩子的 NPC 就不会有回执。
clear
cmd /say 你好
wait text 躬身作了个揖 timeout=10000
sleep 500
cmd /t 0

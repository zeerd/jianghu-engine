# drink_and_faint.sce — 山脚：饮烈酒 → 醉意逐拍推进 → 昏过去 → 自己醒过来
#
# 覆盖：
#   item/sample_jiu.lua（出厂即满的 alcohol 液体）+ lua-default/item/liquid_base.lua
#     的 do_use 醉酒分支：喝下即挂 condition/drunk 状态（liquid/type=alcohol
#     才走这条，装清茶的酒壶走不到——两者在同一条 do_use 上分岔）
#   condition/drunk.lua 的四拍推进：init 播报 → 逐拍递减 → 拍数到 1 调
#     unconcious() 昏过去 → 拍数到 0 调 revive() 醒过来并回满气血
#
# 判定（§8.4①，全部本人私发或状态，不看房间播报）：
#   · 「好象是喝醉了」是 message_vision 对本人的渲染——这坛酒全服只有本道会喝；
#   · 「你的眼前一黑」由 unconcious 经 message_vision_room(me,"tell") 私发，
#     「慢慢地你终于又有了知觉」同为 revive 的私发，两者都只到本人；
#   · 苏醒后断 attr hp > 0：unconcious 把气血归零，光有回执说明不了真醒过来，
#     状态断言才钉得住「醒」这件事本身。
#
# 位置与动线：本段全程待在青云山山脚、一步不挪——昏过去期间任何命令都会被
#   禁用操作挡掉，中途换房必然卡死。排在 wield_sword 之后（此时仍在山脚），
#   下一段 tea_fill_drink 的第一句就是 /go 山脚茶棚，故本段必须收在山脚。
# 前置：buy_sell.sce 已买下烈酒（商品 14）。

cmd /t lie jiu
clear
cmd /use
# 饮用回执（本人版）——喝下去了，状态才谈得上挂载
wait text 好象是喝醉了 timeout=10000
# 昏过去：私发。首拍间隔最长约 14 秒、其后每 5~9 秒一拍，窗口给足
wait text 你的眼前一黑 timeout=60000
# 醒过来：私发。状态自己在下一拍收口，赶在引擎的自动苏醒兜底之前
wait text 慢慢地你终于又有了知觉 timeout=60000
wait attr hp > 0 timeout=10000
cmd /t 0

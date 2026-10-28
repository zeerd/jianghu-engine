# mirror_dummy.sce — 半山腰：练功木人（type="mirror"，NpcMirror 基类）
#
# 覆盖：
#   lua-sample/npc/sample_wooden_dummy.lua 的 do_talk 两条分支：
#   · 对练问话 → self:make_mirror(me, 8) 照挑战者抄一遍（脚本中途报错会被
#     CLuaNpcT::do_talk 回退给 base_do_talk，本步那句私发应答就不会出现）；
#   · 其余问话 → base_do_talk → 本脚本未注册询问 → notify_fail 私发。
#   以及 room/sample_mountain_mid.lua 的 add_npc("sample_wooden_dummy")。
#
# 并行说明：
#   · 应答走 say(msg, me) 私发、失败回执走 notify_fail，都只有本账号收得到；
#     木人只被本道触碰，别道（L4 同在半山腰）既不对它说话也不打它。
#   · 「对练」改的是木人自己，进不了任何人的断言，故不构成共享状态。
#   · `/t 练功木人` 是本道自己的目标，块尾 `/t 0` 收干净，不给后面片段留
#     隐式目标（谁接着用谁遭殃，见「/t 目标是隐式状态」）。

cmd /t 0
wait room 青云山半山腰
wait object 练功木人

cmd /t 练功木人
clear
cmd /ask 对练
wait text 照着你的起手式摆开了门户 timeout=10000
sleep 600

# 非对练问话：脚本交回 base_do_talk，未注册询问 → 私发失败回执
clear
cmd /ask 闲聊
wait text 对方并不愿意理你 timeout=10000
sleep 600
cmd /t 0

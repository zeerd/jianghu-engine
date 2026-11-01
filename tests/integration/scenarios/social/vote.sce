# vote.sce — L4（社交道）：频道投票入口与计票链
#
# 覆盖（P5 收口批次，裁定见 design §6「单机 E2E 结构性不可达」行）：CCmdUser::vote 的**入口/门槛/计票/去重**分支
#   + determine_total / counting 的同 IP 去重逻辑：
#   · level 门槛负测：VOTE_LEVEL_LIMIT=5，新号 level=1 →「你等级不到5级…」
#     （age 门槛 VOTE_AGE_LIMIT=15 在 config start_age=16 下不可达——与
#      channel_and_banword.sce 同口径，E2E 不测低龄分支）；
#   · 领师父历练（山脚老李头 set_inquiry_func 回调，每号一次 5 万）+ /upgrade×4
#     升到 5 级——vote 的 level 门槛是 E2E 唯一可达的门槛分支；
#   · 投自己（/t $acct）「close chat」：determine_total 按在线合格人数/3 且
#     **每 IP 只计 1 票**、下限 3 票——E2E 全账号同机同 IP，恒得 total=3、
#     counting=1 → 走 diff>0 的**未达标播报**分支「需要3票通过，还差2票」；
#   · 重复票去重：同 myvariable（close_chat_<目标id>）再投 →「你已经投过相同的票了」；
#   · 非法频道名：chat/party/new 之外 →「不能打开gm这个频道」。
#
# **不可达分支（记录不测）**：
#   · 投票通过分支（设置/清除 block_key + 成功播报）：需 ≥3 票而同 IP 上限 1 票，
#     单机 E2E 结构性不可达（多机多 IP 环境才可达）；
#   · nullify_vote 超时清除：call_out 600 拍（10 分钟），超出 E2E 时长预算；
#   · age<15 拒绝分支：start_age=16 恒过（同 channel_and_banword 口径）。
#
# 断言纪律（§8.6①）：门槛/重复票/非法频道全为 notify_fail 私发；未达标播报虽是
#   全服 message("vote")，但文本含「【表决】…需要N票通过」高度特异，只有本片段
#   wait 它、且 total 恒 3（同 IP 去重后与其余道等级无关），不会被其他道误满足。
# 动线：换房回山脚领历练前先 wait object 老李头（/t 英文 id 依赖客户端对象名录，
#   ADDOBJECT 与 GOTOROOM 可分批到达——同 provision 片段的竞态口径）。

cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
wait object 老李头

# --- 领师父历练（老李头「师父」问询回调，每号一次 5 万；到账由下游升级验证） ---
cmd /t lao li
clear
cmd /ask 师父
cmd /t 0

# --- level 门槛负测（当前 level=1 < 5）：私发拒绝回执 -------------------------
cmd /t $acct
clear
cmd /vote close chat
wait text 你等级不到5级，没有投票权。 timeout=10000

# --- 升级 1→5（历练到账即「升级成功」×4；未到账则首个 upgrade 就超时暴露） ---
repeat 4
clear
cmd /upgrade
wait text 升级成功 timeout=10000
end

# --- 投自己 close chat：同 IP 上限 1 票 + 下限 3 票 → 未达标播报（diff=2） -----
clear
cmd /vote close chat
wait text 需要3票通过，还差2票 timeout=10000

# --- 重复票去重：同一动议（close_chat_<目标>）同人再投 → 拒绝 ----------------
cmd /vote close chat
wait text 你已经投过相同的票了。 timeout=10000

# --- 非法频道名：chat/party/new 之外一律拒绝 ----------------------------------
cmd /vote open gm
wait text 不能打开gm这个频道。 timeout=10000
cmd /t 0

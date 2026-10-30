# divorce_a.sce — 离婚（A 方：取消确认分支 + 确认离婚 + couple 删除复核）
#
# 与 divorce_b.sce 配对并行。月老离婚链：do_divorce_info（双方在线校验）
#   → 菜单「我决心已下 / 再考虑考虑」→ do_confirm_delcouple：
#   双方 couple 删除 + couple_betray+1 + message_vision 播报。
#
# 分支覆盖：
#   取消确认（menu「再考虑考虑」→ handle_action cancel，无副作用）
#   确认离婚（第二次咨询后「我决心已下」）
#   couple 删除复核：随后 /pair 落「这不太合适吧？」（pair 入口的 couple 校验
#   反向证明 couple 映射已不存在——E2E 无 attr 条件可直读 couple）。
#   未婚拒绝分支在结婚前已由 marriage_a.sce 覆盖；伴侣离线分支需一方 /quit、
#   破坏配对道结构，不测（见计划文档）。

# 离婚咨询（已婚分支）：先断私发问句，再选「再考虑考虑」取消
cmd /t yue lao
clear
cmd /talk
wait menu timeout=10000
menu 离婚登记
wait text 确定要离婚吗 timeout=10000
wait menu timeout=10000
menu 再考虑考虑

# 重新咨询并确认离婚（取消分支已确认无副作用后才执行）
clear
cmd /talk
wait menu timeout=10000
menu 离婚登记
wait menu timeout=10000
menu 我决心已下
wait text 你与$peer解除了夫妻关系 timeout=15000

# couple 删除复核：夫妻组队不再成立（pair 入口 couple 校验失败，私发 notify_fail）
cmd /t $peer_acct
clear
cmd /pair
wait text 这不太合适吧 timeout=10000
cmd /t 0

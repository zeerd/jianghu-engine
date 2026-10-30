# marriage_a.sce — 结婚（A 方：领表/签名/交表，覆盖 marry 成功链与四个错误分支）
#
# 与 marriage_b.sce 配对并行。流程见 lua-sample/item/sample_marriage_form.lua
#   两段签名：A 领表签名 → 交同房间 B → B 持表离房触发「签名人不在场」驳回
#   → B 回房完成第二签 → couple 建立（房间播报收口，A 直接断言该播报）。
#
# 分支覆盖：
#   1) 未婚拒绝离婚咨询（月老 do_divorce_info 未婚分支，先做——结婚后此分支不可达）
#   2) 金额不足领表失败（1 金币不满足 accept_object 的 value>=100 → do_give 拒收）
#   3) 自己重复签名（do_use 的 sign_id == me 分支）
#   4) 签名人不在场（B 侧触发，见 marriage_b.sce）
#   5) 已婚拒绝领表（结婚后再给月老钱）
#
# ⚠ /t 定向纪律：`/t` 只收 ID——玩家用数字账号 ID（全服有效、跨房间），
#   NPC/物品用英文 id（当前环境）；显示名允许重名、不作目标（$peer 只给
#   文本断言用）。本段：同房间定向（月老/登记表/交表）+ 一条跨房间实证
#   （B 在山脚回呼 A，见 B 侧脚本），收敛点是 couple 建立的房间播报。

cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
cmd /go 青云城住宅区
wait room 青云城住宅区 timeout=15000
# /t 的英文 id 在客户端对象名录里解析，住宅区的对象推送晚于 wait room 一拍，
# 先 wait object 再定向（否则 Target not found → talk 无目标 → 菜单等不到）
wait object 月老

# --- 分支 1：未婚拒绝离婚咨询（私发 say，仅本人可见） -----------------------
cmd /t yue lao
clear
cmd /talk
wait menu timeout=10000
menu 离婚登记
wait text 你还没结婚，何来离婚 timeout=10000

# --- 领表：金额不足分支（1 金币 < 100）→ do_give 私发拒收，随后足额重试 -----
clear
cmd /give 金币 1
wait text 对方不要你的东西 timeout=10000

# 足额 100 金币：coin 销毁 + 发登记表（本人版接过播报 + item 真实到账）
cmd /give 金币 100
wait text 你接过月老递来的一张婚姻登记表 timeout=10000
wait item 婚姻登记表 timeout=10000

# --- 第一段签名 + 重复签名错误分支 -------------------------------------------
cmd /t hun yin deng ji biao
clear
cmd /use
wait text 你提起笔，在婚姻登记表上签下了自己的名字 timeout=10000

# 已签过的人不能补第二签（结构上排除自己娶自己）
cmd /use
wait text 你已经签过名了 timeout=10000
cmd /t 0

# --- 交表给 B（同房间）；B 随后离房触发不在场驳回、回房完成第二签 ----------
cmd /t $peer_acct
clear
cmd /give 婚姻登记表
wait text 给$peer数量 timeout=15000
cmd /t 0

# B 在山脚（另一房间）用 /t $peer_acct 定向 A 并回呼——跨房间按数字账号
# ID 定向 + tell 的 E2E 实证：登录花名册交换（SENDUSER→downloaduser→
# DOWNUSER）或 O=0 清理失效时，这条 wait 会红
wait text 不在场签过了 timeout=30000

# couple 建立播报（B 完成第二签；B 的离房-驳回-回房-再签约 10s，宽 timeout）。
# A 是第一签者（message_vision 的 other 侧）：渲染为「<B名>与你…」
wait text 与你在婚姻登记表上签下彼此的名字 timeout=60000

# --- 分支 5：已婚拒绝领表（accept_object 已婚分支，私发 say） ----------------
cmd /t yue lao
clear
cmd /give 金币 100
wait text 你已经结婚了 timeout=10000
cmd /t 0

# bank.sce — 钱庄：余额 / 存款 / 取款 / 超额取款 / 贷款前置门槛（全按具体数额）
#
# 覆盖：CBankOwner::do_balance / do_deposit / do_withdraw / do_loan，
#   含「超额取款」与「有存款时优先命中『你还有存款！』（先于 level 门槛）」两条分支。
#
# 判定：新号的存款（balance）默认就是 0，所以起点与每一步之后都断到具体数额，
#   而不是含糊的「有/没有存款」：
#     初始 0 → 存入 1000 → 1000 → 取 400 → 600 → 超额取款被拒 → 有存款时不可贷款
#   · 存款回执经 message_vision，本人版带「你」（房间版带名字），故取本人版；
#   · 余额查询经 tell 通道只发本人（Message.cpp 的 tell 分支），天然免疫串扰；
#   · 取款/贷款成功只回「完毕」、无数额，所以数额靠随后的 /balance 钉住。
#
# 前置：本道自带现金（起步本金），存款=0 是新角色的真实初始态。

cmd /go 山脚钱庄
wait room 山脚钱庄
wait object 钱庄老板

cmd /t lao ban
clear
cmd /balance
wait text 你现在没有存款 timeout=10000

clear
cmd /deposit 1000
wait text 你存入现款1000金币 timeout=10000

clear
cmd /balance
wait text 你现在共有存款：1000金币 timeout=10000

clear
cmd /withdraw 400
wait text 提款完毕 timeout=10000

clear
cmd /balance
wait text 你现在共有存款：600金币 timeout=10000

clear
cmd /withdraw 99999999
wait text 你没这么多存款 timeout=10000

# 存款未清时贷款被「你还有存款！」抢先拒绝（先于等级门槛）
clear
cmd /loan 100
wait text 你还有存款 timeout=10000

cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚

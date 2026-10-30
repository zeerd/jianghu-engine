# marriage_b.sce — 结婚（B 方：收表/离房触发不在场驳回/回房完成第二签）
#
# 与 marriage_a.sce 配对并行。「签名人不在场」错误分支由 B 自触发：
#   B 持表走到山脚（A 留在住宅区月老旁）→ use → do_use 的 env:present(sign_id)
#   为 nil →「签名之人不在这儿，登记表暂不作数。」（notify_fail 私发）；
#   回房再 use → 第二签成功、登记表焚毁。
#
# ⚠ 同步分层：B 的离房/回房行程自触发（同房间零通信）；唯一的跨房间呼叫
#   是不在场驳回后的回呼 tell——/t $peer_acct 按对方数字账号 ID 全服定向
#   （A 在住宅区、B 在山脚），见下方注释与 marriage_a 头注释。
#
# §8.6④：本方 wait 超时即疑似对端未到位——先查 A 道日志。

cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
cmd /go 青云城住宅区
wait room 青云城住宅区 timeout=15000

# 等 A 领表/签名/交表（A 链约 10s，宽 timeout）
wait item 婚姻登记表 timeout=40000

# --- 离房触发「签名人不在场」（A 在住宅区，B 独自在山脚，必现驳回） ---------
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
cmd /t hun yin deng ji biao
clear
cmd /use
wait text 签名之人不在这儿 timeout=10000

# 跨房间回呼 A：/t $peer_acct（A 的数字账号 ID，全服有效；此刻 A 在住宅区、
# B 在山脚——显示名允许重名、不作定向）。A 侧断言这条私聊到达（实证登录
# 花名册交换 SENDUSER→downloaduser→DOWNUSER 的在线表按 ID 解析可用）。
cmd /t $peer_acct
clear
cmd /tell 不在场签过了
cmd /t 0

# --- 回房完成第二签：couple 建立（B 是第二签者即 message_vision 的 me 侧，
#     渲染为「你与<A名>…」）
cmd /go 青云城住宅区
wait room 青云城住宅区 timeout=15000
# 跨房间回呼把目标切到了 $peer_acct，二次签名前切回登记表（use 需物品目标）
cmd /t hun yin deng ji biao
clear
cmd /use
wait text 你与$peer在婚姻登记表上签下彼此的名字 timeout=15000
cmd /t 0

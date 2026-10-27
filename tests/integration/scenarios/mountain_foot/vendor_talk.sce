# vendor_talk.sce — 商家回退：标准询问流程无条目 → 回绝
# （CLuaNpcT::base_do_talk → CNpc::do_talk_inquiry）：
# 杂货老板是 CVendor（type="vendor"），脚本没登记任何询问条目 → 询问流程直接
# 回绝。老李头这类非商人的 NPC 走的是脚本侧 do_lua_inquiry，两条路互不重叠。
#
# 说明：只由本道触发一次即可覆盖该回退分支。

cmd /t 杂货老板
clear
cmd /ask 商人
wait text 对方并不愿意理你 timeout=10000
sleep 600
cmd /t 0

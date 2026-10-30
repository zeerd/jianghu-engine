# invite_a.sce — 做客邀请（A 方：自行置业成为居民 → 邀请 B 到家做客）
#
# 与 invite_b.sce 配对并行。invite 要求邀请方已是居民（resident 非空），
#   故 A 先复用 house/apply.sce 置业（单人间，地价 2500 金 < 起步本金 60000），
#   再从宅院「出口」步行回会合房间茶棚（B 一直在茶棚等待），邀请 B。
#   房名「雅室」为 per-owner 实例（get_local_house 按 owner id 取），与 L1 无冲突。
#   invite 只建立邀请关系（get_room_char 同屋即可），不传送任何一方。

include ../house/apply.sce

# 置业后 A 在宅院内，经「出口」回住宅区 → 山脚 → 茶棚会合
cmd /go 出口
wait room 青云城住宅区 timeout=15000
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000
cmd /go 山脚茶棚
wait room 山脚茶棚 timeout=30000

# A 邀请 B 做客（A 已是居民），B 收菜单并接受
cmd /t $peer_acct
cmd /invite
wait text 接受了你的邀请 timeout=40000
cmd /t 0

# master_recruit_prep.sce — L5（玩家师父）的收徒资格前置
#
# 覆盖 CCmdStandard::recruit 的两道硬门槛（apprentice 侧读同一个键，同源）：
#   1) 门派与代数：family/family_name 非空 + family/generation 非 0——
#      走设计内路径拜师青云子取「青云门第 2 代」（青云子 create_family 青云门/1 代），
#      与 L2/L4/L8/L10 的 NPC 拜师同一条链；
#   2) 收徒资格：is_user(me) && recruit_exp == 0 →「你还没有收徒资格！」——
#      新人 dbase 的 recruit_exp 恒 0（用户命令 /rexp 只在已有收徒记录时才回填），
#      由 L9 巫师 `call set` 供能（§2.6 服务协议；请求词与 capture 标记全服唯一）。
#
# 段落位置：lane_interaction_a 的 provision 之后、rendezvous 之前——师徒正向流程
#   （recruit_a 起）排在会合之后，本段是它的前置；先完成再进茶棚会合，
#   L6 侧 rendezvous 的 `wait object $peer`（180s）吸收本段耗时。
#   段尾必须回到青云山山脚：茶棚的门只在山脚（go_door 逐跳 present_door），
#   半山腰没有直通茶棚的门。
#
# 并行说明：拜师回执与巫师回执均为私发（§8.6①）；半山腰同时有 L4/L8/L10 在
#   拜师，房间版播报不作本段断言对象。

cmd /t 0
wait room 青云山山脚 timeout=15000
cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=15000
wait object 青云子 timeout=10000

# 拜师青云子：「青云门的弟子」经 say(msg, me) 私发，同房间别的账号收不到
cmd /t qing yun zi
clear
cmd /apprentice
wait text 青云门的弟子 timeout=10000
cmd /t 0

# 巫师供能收徒资格：请求/回执私发，发完即进 wait（§2.6）
cmd /t 1
cmd /tell 收徒资格供能请求，收徒资格供能账号是 $acct
wait text 收徒资格供能完毕 timeout=300000
cmd /t 0

# 回山脚（rendezvous 的 /go 山脚茶棚只认山脚的门）
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000

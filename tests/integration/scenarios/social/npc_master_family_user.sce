# npc_master_family_user.sce — NPC 师父不写 family/user（场景 C：回归保护）
#
# 覆盖：CChar::recruit_apprentice 只在 is_user(master) 时写 family/user=1——
#   NPC 师父（青云子）收徒后该键必须缺失（query 回 0）。
#   它是玩家收徒场景（L5/L6 师徒段）的反向锚点：同一段代码的两条分支各有一侧
#   断言——玩家师父侧见 interaction/rexp_a.sce 的「(弟子)」与 expell_a 的「(----)」。
# 判定：dbase 没有用户侧只读命令，经 L9 巫师 `call query` 直读后私发回执
#   （§2.6 服务协议：回执带值，capture 抓数字回传）。回执到达与值断言分两步：
#   值错时红在第二步，便于区分「没回执」与「值不对」。
# 段落位置：common/apprentice.sce 之后立刻发请求（链首发请求，L9 尽早消费、
#   不推迟冲级等晚到服务），sleep_and_observe 之前（本道随后才进长睡）。

cmd /t 1
cmd /tell NPC师徒家门查询请求，NPC师徒家门查询账号是 $acct
wait text NPC师徒家门查询完毕 timeout=300000
wait text family/user=0 timeout=10000
cmd /t 0

# rexp_a.sce — 收徒经验查询 /rexp（场景 D：玩家师父查玩家徒弟）
#
# 前置：recruit_a/recruit_b 已建立师徒关系且 B 已 /save——CCmdUser::recruit_exp
#   经 g_channel.query_recruit 直读 recruit_exp 表，记录由 CharSave::
#   save_to_server 的 family/user 分支写入（B 不存盘就没有这行记录）。
# 断言：
#   ·「当前你有徒弟1个」——记录条数（CMapping 每个徒弟一个顶层键，get_count=1）；
#   ·「徒弟<peer 名>(<peer 数字账号>)贡献经验：0。(弟子)」——名字 + 账号 id +
#     贡献经验 + **弟子标记**：该标记只在 family/user=1 时才被存盘链写成 1，
#     即场景 A 的 family/user=1 证据（反向对照见 expell_a 的「(----)」）；
#   · 贡献经验 0：B 存盘时 combat_exp 与 family/last_exp 同为 0（本道尚未历练）。
#
# 同步：等 B 的「徒弟存好了」私发再查，保证记录先落库、查询后执行（§8.6①）；
#   本段各回执均为首次出现，不 clear。

wait text 徒弟存好了 timeout=30000

cmd /rexp
wait text 当前你有徒弟1个 timeout=10000
wait text 徒弟$peer($peer_acct)贡献经验：0。(弟子) timeout=10000

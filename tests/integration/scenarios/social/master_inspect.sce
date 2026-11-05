# master_inspect.sce — 师父验看：门规四问 + 实学报数 + 容貌比较
#
# 覆盖：
#   l_query_str / l_query_con / l_query_int / l_query_dex（门规四问的合格线
#   判定）、l_query_per（与老夫比较容貌）、l_query_combat_exp（历练）、
#   l_query_valid_exp（可用经验，玩家侧读取）。
#
# 前置状态是本道正常玩法攒出来的，报数断言精确值：
#   · 历练 51000 = 天下大考答对 1000（exam.lua，channel_and_banword.sce）
#     + 老李头「师父」问路 50000（vote.sce，每号一次）——vote 头注的
#     「到账由下游升级验证」在本片段变成精确值验证；
#   · 可用经验 49700 = 51000 − level2_exp(5)=1300（vote 的 /upgrade×4 升到
#     5 级，见 lua-default/data/level_exp.lua）− sk_exp 0（本道不学技能）。
#     两数不等——等级已耗真扣在公式里，query_valid_exp 若读错源（如照抄
#     总经验）报数对不上；
#   · wait attr combat_exp == 51000 先钉服务端状态基线（vote 末次升级的
#     flush_my_info 已把快照推到客户端），报数与 attr 两个来源互证。
#
# 判定（全私发，§8.6①）：
#   · 「门规尽过」行只有四问全过才发——回绝支不到场，该行出现即证明
#     query_str/con/int/dex 四个读数都 ≥ 20；
#   · 报数行是精确值断言（51000/49700 推导见上）；
#   · 「论容貌你胜过老夫」：老夫容貌钉 0（sample_master.lua create），
#     弟子创建下限 10（SetupChar 的 random(21)+10）恒胜，比较不随随机
#     属性漂。
#
# 顺序与动线：接 vote.sce 之后——历练已入账、升级已落，且本道此时仍在
#   山脚；验看在半山腰，看完回山脚，道尾判师（betray_and_repent.sce）
#   以山脚起步。
#
# 并行（§8.6①）：门规/报数/容貌三行都是青云子的私发 say；半山腰同房的
#   L2 只认铁人桩/技能串，L5/L6 收徒链问话不同（「验看」是本片专属问话）。

cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=15000
wait object 青云子 timeout=10000

# 状态基线：本道正常玩法后的历练精确值（大考 1000 + 问路 50000）
wait attr combat_exp == 51000 timeout=10000

cmd /t qing yun zi
clear
cmd /ask 验看
wait menu timeout=10000
menu 请师父验看根骨
wait text 门规尽过——根骨、悟性、身法都够格 timeout=10000
wait text 历练51000点，可用经验49700点 timeout=10000
wait text 论容貌你胜过老夫 timeout=10000
cmd /t 0

# 回山脚：道尾判师片段以山脚起步
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000

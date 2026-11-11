# master_inspect.sce — 师父验看：门规四问 + 实学报数
#
# 覆盖：
#   l_attr（门规四问的合格线判定）、query_valid_exp（可用经验，前导组合），
#   玩家侧读取）。
#
# 前置状态是本道正常玩法攒出来的，报数断言精确值：
#   · 历练 51000 = 天下大考答对 1000（exam.lua，channel_and_banword.sce）
#     + 拜师入门礼 50000（sample_master.lua attempt_apprentice，本道
#     line 22 common/apprentice.sce 已领）——vote 头注的
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
#     attr("str"/"con"/"int"/"dex") 四个读数都 ≥ 20；
#   · 报数行是精确值断言（51000/49700 推导见上）。
#
# 顺序与动线：接 vote.sce 之后——历练已入账、升级已落，且本道此时仍在
#   山脚；验看在半山腰，看完回山脚，道尾判师（betray_and_repent.sce）
#   以山脚起步。
#
# 并行（§8.6①）：门规/报数两行都是青云子的私发 say；半山腰同房的
#   L2 只认铁人桩/技能串，L5/L6 收徒链问话不同（「验看」是本片专属问话）。

cmd /go 青云山半山腰
wait room 青云山半山腰 timeout=15000
wait object 青云子 timeout=10000

# 状态基线：本道正常玩法后的历练精确值（大考 1000 + 拜师 50000）
wait attr combat_exp == 51000 timeout=10000

cmd /t qing yun zi
clear
cmd /ask 验看
wait menu timeout=10000
menu 请师父验看根骨
wait text 门规尽过——根骨、悟性、身法都够格 timeout=10000
wait text 历练51000点，可用经验49700点 timeout=10000
cmd /t 0

# 回山脚：道尾判师片段以山脚起步
cmd /go 青云山山脚
wait room 青云山山脚 timeout=15000

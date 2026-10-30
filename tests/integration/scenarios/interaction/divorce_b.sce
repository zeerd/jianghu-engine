# divorce_b.sce — 离婚（B 方：断言双方 couple 解除的房间播报）
#
# 与 divorce_a.sce 配对并行。message_vision("$N与$n解除了夫妻关系。", A, B)
#   对 B 渲染「<A名>与你解除了夫妻关系。」——含「你」为本人版（§8.4②）。
#   B 不需其他动作，等播报即可（A 侧的取消分支与 /pair 复核在 A 道）。

wait text 与你解除了夫妻关系 timeout=40000

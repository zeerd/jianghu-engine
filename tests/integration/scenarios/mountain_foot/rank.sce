# rank.sce — 排行榜查询（paihangbang 全局绑定）
#
# 覆盖：SqlRank.cpp 的 paihangbang 查询 + sample_guide.lua 的 handle_action
#       "update" 行消费。查询成功且无人达标时脚本按 0 条播报。
#
# 达标门槛是 cc_experice > 520000（SqlRank.cpp），而本世界各道能拿到的历练
# （起步 1000 + 问路 50000 + 任务 3000 + 杀山鸡 10000）远够不着，故断言
# 「无人达标」在多账号并行下恒成立。
#
# ⚠ 并行隔离：应答走 say(msg) 无目标 → message_vision_room 房间播报，
#   同房间的账号都会收到。因此本片段只由 L1 一道触发并断言；其他道不执行
#   /ask 排行榜，也就不会与 L1 的等待互相串扰。

cmd /t lao li
cmd /ask 排行榜
wait text 排行榜上还没有人达到上榜资格 timeout=10000
cmd /t 0

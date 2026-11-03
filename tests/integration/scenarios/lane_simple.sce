# lane_simple.sce — 简单模式（冒烟）道：网络联通 / 行走 / look
#
# ⚠ 定位：仅供人类做快速确认（「服务器还活着吗」），AI/Agent 禁用——AI 的验证
#   与交付验收必须跑完整模式，不许拿本道的绿当验收结论。
#
# 职责：整条道只验证最小命令面——服务器起来了、TCP 连得上、登录回包、/look 吐出
#   房间描述、/go 能换房。它是改动后的快速自检入口（-Simple，整段墙钟预算 30s），
#   不承担功能回归与覆盖率——那是完整模式（其余 lane_*）的口径。
#
# 账号机制：巫师一号（cc_id=1，服务端启动自举，同 L9）——本道 NoRegister，完全
#   跳过注册阶段。简单模式每次重建测试库（run-integration-tests.ps1 派生默认值）：
#   存档里的 start_city 是上次离线房间，不清库时首登落点不确定，common/login.sce
#   的起始房间断言会漂；重建库后首登走新人分支必落 config 的 start_room，本道
#   片段链在任何历史状态下都确定。
#
# 收口：道尾不挂 common/quit.sce——quit 触发全量落库、会把单线程工作线程停顿
#   数秒，冒烟道不起这个头；CLI 进程退出即断连（L2~L10 同例）。

include common/login.sce
include common/walk_and_look.sce

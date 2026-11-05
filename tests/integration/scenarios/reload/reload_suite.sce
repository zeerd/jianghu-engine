# reload_suite.sce — 热重载试验台剧本链入口
# （run-integration-tests.ps1 的 reload 试验台：独立服 6613，与各道并行）
#
# 职责：验证「改脚本文件 → 心跳自动重载 → 行为切换」主链路，以及失败兜底、
# 手动 reload 回执、共享模块（require 缓存）语义、mtime-noop 不误触发。
# 所有文件改动都用 filecopy/filedelete 写在片段里（落点钉死在 $scripts
# 私有副本），改文件 → 等重载结果（sys 广播）→ 断言新行为的因果同处一个
# 片段，读片段即见因果。
#
# 前置 vars（reload_testbed.ps1 注入）：
#   acct/pass  巫师账号（服务端自举 grant≥80：reload 命令与 sys 频道都需要）
#   fixtures   只读变体目录 tests/integration/reload_fixtures
#   scripts    私有脚本副本 test_tmp/e2e_reload/scripts（filecopy 唯一落点）
#
# 顺序即状态链，不可重排：NPC A→B→(损坏/修复)→C、房间 A→B→A、
# 共享模块 A→B；每个片段依赖前一片段留下的文件状态与行为基线。
# 注意：加载器不剥行尾注释（只认整行 #），include 的理由一律写在上方独立行。

include ../common/login.sce
# 必须最先：订阅 sys 才收得到自动重载结果广播
include sys_subscribe.sce
# 全 A 版基线（房/物/NPC 三路甲标记）
include baseline.sce
# 自动重载主路径（NPC）
include auto_npc.sce
# 物品实例刷新
include auto_item.sce
# 房间实例刷新（reset 钩子通道）
include auto_room.sce
# 语法错误兜底 + 失败广播边沿不刷屏
include fail_syntax.sce
# 修复恢复：清错误态，下一次失败才会再广播
include heal_npc.sce
# 运行时错误兜底
include fail_runtime.sce
# 同结构恢复第二段（d 变体：与 c 行为同为丙、字节必变，见 heal_npc_d 头注）
include heal_npc_d.sce
# 文件缺失兜底
include fail_missing.sce
# 手动 reload 失败回执必须如实
include cmd_fail.sce
# 手动 reload 成功回执 + 新行为
include cmd_ok.sce
# reload all 部分失败：回执带计数、其余不失效
include reload_all_error.sce
# 共享模块：自动重载换不掉 require 缓存，reload all 才换
include shared_base.sce
# 原样重写不误触发（探针 + 同拍真实变更做屏障）
include noop.sce
# 道收口：主动退出，之后不得再有任何片段
include ../common/quit.sce

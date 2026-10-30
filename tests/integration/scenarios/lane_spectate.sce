# lane_spectate.sce — L7 道：观战（PK 与死亡链的第三方账号）
#
# 片段链：登录 → 在会合房间待命收开战信号（spectate）。
# PK 准备（pk_prep：历练/装备/冲级）只打在赢家账号 A 上，本道不参与——
#   本道的职责就是「房外第三方」，不引入任何共享资源耦合。
#
# 职责：L5/L6 两账号在战斗房内分出生死时，本道作为**房外第三方**补齐
#   「观战与加入」面（计划书 P4「可借第 3 个互动账号或复用道」的第三个账号）：
#   比试房观战（guard）与观战退出（halt 观战分支）、PK 战斗房加入
#   （join 成功分支 + ready_pk，留场至散场）、随 end_fight 散出。
#   本道只在山脚茶棚待命、收 A 侧两条 tell 信号推进，不参与组队/私聊/交易
#   等其余互动，也不做任何资源操作。
#
# peer 机制（run-integration-tests.ps1 注入）：
#   本道 vars：peer/peer_acct = L5（A）——回信号定向与文本断言用；
#   L5 vars：spectator/spectator_acct = 本道——A 跨房 tell 开战信号用。
#   两键都由注册后回读 acct 注入，禁止硬编码角色名或账号。
#
# 稀缺资源归属声明（§8.4 / §8.6 ⑤）：本道不触碰任何钉给唯一道的资源，
#   也不做任何采购（PK 准备归赢家账号）；
#   战斗房是 per-kill 实例（iid=fight_room，随开战创建、随人员散尽销毁），
#   不占用共享世界的任何对象。
#
# 超时口径：本道任一 wait 超时，先看 L5 日志（A 侧未发信号 / 比试未成立，
#   §9「疑似对端未到位」）；L5 侧对应等待（比试观战完毕）超时则反查本道日志。

include common/login.sce
include interaction/spectate.sce

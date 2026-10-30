# lane_interaction_a.sce — L5 道：配对互动 A
#
# 职责：与 L6 道配对，覆盖多客户端互动场景（设计见
#   docs/design/integration-test.md §2.1）。A 扮「主动方」——邀请组队、发起私聊、定向说话/表情、赠送物品、
#   置业后邀请 B 做客、婚姻登记表签名领表、夫妻组队、离婚确认、摆摊售卖、
#   发起切磋与玩家 PK（对手是 L6，观战联动 L7）。
#
# 片段链：登录 → PK 前置（pk_prep：起步历练 + 青锋剑 + 冲 10 级，赢家账号
#   专属）→ 会合 → 组队 → 私聊 → 说话表情 → 赠送 → 做客 → 结婚 → 夫妻组队
#   → 离婚 → 摆摊交易 → 切磋（fight/follow/halt）→ 玩家 PK 与死亡链（pk_*）。
#   marriage/pair/divorce/trade/pk 前置依次依赖：pair 要 couple、divorce 要
#   couple、trade 双方需同房间；pk 段依赖观战道（L7）在线接信号，链内自有
#   步行与 tell 同步。
#
# peer 变量：由 run-integration-tests.ps1 注册阶段注入——`peer`=对方角色名
#   （仅文本断言用）、`peer_acct`=对方数字账号 ID（供 /t 定向，/t 只收 ID），
#   禁止硬编码对方角色名或账号（runhex 每轮都变）。
# spectator 变量：`spectator`=观战道（L7）角色名、`spectator_acct`=其数字
#   账号 ID（本道 WitnessId=7 反向注入）——pk/fight 段跨房 tell 开战信号用。
#
# 稀缺资源归属声明（§8.4 / §8.6 ⑤）：本道与 L6 道**不得触碰**钉给唯一道的
#   资源——山脚木箱与药箱（L1）、山顶树箱经验果与山鸡与副本（L3）、
#   排行榜房间播报（L1）、断肠散与传送符（L4）；自购补给走本道自己的
#   provision 与 pk_prep 片段（金创药 1 包用于赠送、青锋剑 1 柄为 PK 武装，
#   均按商人常规库存自购）；摆摊段的贸易文书按人发放、
#   待售商品（金创药/茶壶）本道自购，均不占稀缺资源。

include common/login.sce
include interaction/pk_prep.sce
include provision/interaction_provision_a.sce
include interaction/rendezvous.sce
include interaction/party_a.sce
include interaction/tell_a.sce
include interaction/say_emote_a.sce
include interaction/give_a.sce
include interaction/invite_a.sce
include interaction/marriage_a.sce
include interaction/pair_a.sce
include interaction/divorce_a.sce
include interaction/trade_a.sce
include interaction/fight_a.sce
include interaction/pk_a.sce

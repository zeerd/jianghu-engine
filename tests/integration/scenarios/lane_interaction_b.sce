# lane_interaction_b.sce — L6 道：配对互动 B
#
# 职责：与 L5 道配对（PeerId 互注，注册阶段互相注入 vars：`peer`=对方角色名
#   仅供文本断言、`peer_acct`=对方数字账号 ID 供 /t 定向——/t 只收 ID）。
#   在互动场景里扮「响应方」——接受组队、跟随/离队、收屏蔽后的
#   私聊、收定向说话/表情、收赠送物品、接受做客邀请、受邀同睡（wakeup2
#   女性分支，本道注册为女性）、应战切磋，
#   并在 PK 终局承担死亡→复活链（判定见 pk_b.sce 头注）。
#   本道无需自购补给（全部为接收方），故不设 provision 片段。
#
# 性别：本道注册为女性（vars 的 gender 键）——wakeup2 双人同睡要求异性，
#   A（L5）男性、B（L6）女性；除睡眠分支文案外，全链断言均为私发/本人版
#   或含对方名渲染，与性别无关（设计见 docs/design/integration-test.md §2.1）。
#
# 稀缺资源归属声明（§8.4 / §8.6 ⑤）：本道与 L5 道**不得触碰**钉给唯一道的
#   资源——山脚木箱与药箱（L1）、山顶树箱经验果与青蛇与副本（L3）、
#   排行榜房间播报（L1）、蛇胆与传送符（L4）。

include common/login.sce
include interaction/rendezvous.sce
include interaction/party_b.sce
include interaction/tell_b.sce
include interaction/say_emote_b.sce
include interaction/give_b.sce
include interaction/invite_b.sce
include interaction/marriage_b.sce
include interaction/pair_b.sce
include interaction/divorce_b.sce
include interaction/trade_b.sce
include interaction/sleep_b.sce
include interaction/fight_b.sce
include interaction/pk_b.sce

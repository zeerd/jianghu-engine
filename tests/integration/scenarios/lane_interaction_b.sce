# lane_interaction_b.sce — L6 道：配对互动 B
#
# 职责：与 L5 道配对（PeerId 互注，注册阶段互相注入对方角色名到 vars 的
#   peer 键）。在互动场景里扮「响应方」——接受组队、跟随/离队、收屏蔽后的
#   私聊、收定向说话/表情、收赠送物品、接受做客邀请。
#   本道无需自购补给（全部为接收方），故不设 provision 片段。
#
# 稀缺资源归属声明（§8.4 / §8.6 ⑤）：本道与 L5 道**不得触碰**钉给唯一道的
#   资源——山脚木箱与药箱（L1）、山顶树箱经验果与山鸡与副本（L3）、
#   排行榜房间播报（L1）、断肠散与传送符（L4）。

include common/login.sce
include interaction/rendezvous.sce
include interaction/party_b.sce
include interaction/tell_b.sce
include interaction/say_emote_b.sce
include interaction/give_b.sce
include interaction/invite_b.sce

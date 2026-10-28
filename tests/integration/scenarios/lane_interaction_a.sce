# lane_interaction_a.sce — L5 道：配对互动 A
#
# 职责：与 L6 道配对，覆盖多客户端互动场景（CmdUserParty 组队链、私聊与
#   屏蔽、定向说话/表情、玩家间赠送、做客邀请等，见 docs/e2e_interaction_plan.md
#   P3 阶段）。A 扮「主动方」——邀请组队、发起私聊、定向说话/表情、赠送物品、
#   置业后邀请 B 做客。
#
# 结婚/离婚段（marry/divorce 及 pair 夫妻组队）本轮跳过：结婚流程已改走
#   「婚姻登记表」两段签名（lua-sample/item/sample_marriage_form.lua，
#   无 /marry 命令，不再依赖 marry_target），对应剧本片段尚未编写，
#   连带 CmdUserParty 的 pair* 函数暂不覆盖——按计划下一轮补 E2E。
#
# peer 变量：由 run-integration-tests.ps1 注册阶段注入（peer=对方角色名），
#   剧本一律用 $peer 引用，禁止硬编码对方角色名（runhex 每轮都变）。
#
# 稀缺资源归属声明（§8.4 / §8.6 ⑤）：本道与 L6 道**不得触碰**钉给唯一道的
#   资源——山脚木箱与药箱（L1）、山顶树箱经验果与山鸡与副本（L3）、
#   排行榜房间播报（L1）、断肠散与传送符（L4）；自购补给走本道自己的
#   provision 片段（仅金创药，用于赠送）。

include common/login.sce
include provision/interaction_provision_a.sce
include interaction/rendezvous.sce
include interaction/party_a.sce
include interaction/tell_a.sce
include interaction/say_emote_a.sce
include interaction/give_a.sce
include interaction/invite_a.sce

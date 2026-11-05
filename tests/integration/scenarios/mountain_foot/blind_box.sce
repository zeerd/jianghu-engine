# blind_box.sce — 盲盒：xrandom 从奖池抽一件（use 拆盒）
#
# 覆盖：
#   · src\util\Random.cpp 的 l_xrandom——item/sample_blind_box.lua 拆盒时
#     用它抽奖池下标（奖池按 value 升序，xrandom 偏向小值，越贵越难开出）；
#   · DB 链（模块 lua-sample/data/box_draw_log.lua，独立表 sample_box_draw
#     记谁/何时/开出什么）：建表登记 l_db_register_ddl + 方言 l_get_db_type
#     在开机期由 data/hooks.lua 的 require 触发（早于 prepare_tables），
#     拆盒记档走 l_db_execute 写 + l_db_query 读回；
#   · hooks.post_buy（data/hooks.lua）的成交须知走 l_message_vision_self：
#     只送买卖双方、不进房间广播，按 goods.basename 收窄到盲盒。
#
# 判定：
#   · 买入回执是本人版 message_vision（「你从杂货老板那里买下了…盲盒」）——
#     山脚多账号并行采购时，别人的成交串不进本道的「你…」断言；
#   · 成交须知是盲盒专属私发，且按 basename 收窄：其余各道买的是别的货位，
#     这句既不发给他们，也不落进他们的缓冲；
#   · wait item 断盲盒真进背包（回执先于 move 到达，/t 依赖背包同步落定，
#     步骤旁注有说明）；
#   · 拆盒分两步断：「你掀开盲盒的盖子」证明开盒因果（load_item 失败走
#     「盲盒受了潮」拒绝支，这句永远不会出现，超时即红）；紧接的 or 串把
#     奖池 13 件的可能落点全列出来——奖品名必须命中其中之一，证明
#     xrandom 取标 → load_item → move 给的确实是奖池内物品，而不是任意
#     文案。奖名逐字取自各物品 set_name（蛇胆/酒壶/包子/铁剑/铜钥匙/
#     金创药/回春丹/经验果/内功秘籍/剑法秘籍/传送符/棋子/青锋剑）。
#
# 顺序：接 buy_sell 之后——本道仍在山脚、刚从商人处采购完（开箱取物在链前，
#   盘缠已足）。盲盒是货架第 13 号商品（m_code，上架顺序固定、售罄才下架），
#   只有本道买它，其余道的采购不影响它的商品号。
#
# 并行（§8.4①）：买入回执与记档回执对买主都是本人版渲染（「你…」起头，别道
#   账号发不出同样的句子），成交须知只私发买卖双方、不进房间广播；拆盒产物归
#   本道自己，不碰任何钉给唯一道的山脚资源（木箱/药箱/锁箱另有归属）。

cmd /t huo ji
clear
cmd /buy 13
wait text 你从杂货老板那里买下了数量1的盲盒 timeout=10000

# hooks.post_buy 的成交须知（message_vision_self，只送买卖双方、不进房间
# 广播）：钩子先于「买下了」回执发出，而 wait 扫的是整段缓冲，放回执之后
# 照样命中
wait text 盲盒拆开之前不退不换 timeout=10000

# 回执先于 move 到达（CVendor::do_buy 先 message_vision 后 move），/t 抢在
# 背包同步之前会落「Target not found」——wait item 等同步落定（§8.1 条件
# 同步），同时它本身就是「盲盒真进了背包」的状态断言（§8.4①）
wait item 盲盒 timeout=10000
cmd /t mang he
clear
cmd /use
wait text 你掀开盲盒的盖子 timeout=10000
wait text 从里面拿出了蛇胆|从里面拿出了酒壶|从里面拿出了包子|从里面拿出了铁剑|从里面拿出了铜钥匙|从里面拿出了金创药|从里面拿出了回春丹|从里面拿出了经验果|从里面拿出了内功秘籍|从里面拿出了剑法秘籍|从里面拿出了传送符|从里面拿出了棋子|从里面拿出了青锋剑 timeout=10000
# 记档回执 = 独立表 INSERT + COUNT 读回全链（db_execute/db_query，模块见
# data/box_draw_log.lua）；角色名带 runhex 每轮唯一，累计次数恒 1
wait text 此盒已记档：你第 1 次开盲盒 timeout=10000
cmd /t 0

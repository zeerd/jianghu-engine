# locked_box.sce — 山脚锁着的宝箱：锁态 look 隐藏内容 → 无钥被拒 →
#   买钥匙 → 密码吻合开锁 → 取出箱中秘籍
#
# 覆盖（原认领段重开，Box 条目）：
#   l_locked（Box.cpp:297）+ CBox::locked（Box.cpp:16）——山脚房间
#     create 时 box3:Locked("一把铜锁锈得发红", 0)（服务端启动即执行）；
#   CBox::handle_action 的钥匙支（Box.cpp:71-98）：
#     · 无钥匙 open → 「你无法打开。」（私发）
#     · 密码吻合 → un_locked() + 房间播报「只听咔的一响…」
#     · 密码不符支留单测（test_box_door HandleActionKeyWithWrongPasswordKeepsLock）
#   CItem::do_look 的锁态分支（Item.cpp:181-183）：status==locked 时不渲染
#     「里面有：」菜单，开锁后渲染——同一 look 指令锁前锁后两个断言。
#   owner 不符支（Box.cpp:73-77）留单测（HandleActionOwnerMismatchRefusesOpen）
#   ——锁箱无主（Locked 不传 owner），E2E 无从构造非主人。
#
# 判定：
#   锁态 look → notext「里面有：」（锁着就该看不见内容，§8.4② 时间窗 3s）
#   空手 open → 「你无法打开。」（notify_fail 私发）
#   买钥匙   → 本人版「你从杂货老板那里买下了数量1的铜钥匙」
#              （商品表第 11 项——carry_good 顺序追加，前 10 项序号不动，
#               各道 /buy 序号不受影响）
#   持钥 open → 房间播报「只听咔的一响」——该句只有开这把锁才会出现，
#              全服只有本道持此钥匙（§8.4③ 锁箱与钥匙归 L1 独占）
#   再 look   → wait menu → menu 剑法秘籍 → wait item（开锁后内容真可取）
#   持钥再上锁 → 锁态 look 不渲染内容 → 持钥再开锁 → 取书原样成立
#   （钥匙双向可用：上锁/开锁两支回执均私发，与片首 C++ 开锁支互为对照）
#
# 前置：buy_sell.sce 已在商人处采购（本段随后自行买钥匙）。
# 目标纪律（§8.5）：/t suo xiang、/t huo ji 各就各位，段尾 /t 0。

cmd /t 0
cmd /t suo xiang
wait object 锁着的箱子 timeout=10000

# 锁态：look 不渲染内容菜单（3s 时间窗内不得出现「里面有：」）
clear
cmd /look
wait notext 里面有： timeout=3000

# 空手 open：钥匙支的拒绝分支
clear
cmd /open
wait text 你无法打开 timeout=10000

# 买钥匙（商人商品第 11 项）
cmd /t huo ji
clear
cmd /buy 11
wait text 你从杂货老板那里买下了数量1的铜钥匙 timeout=10000

# 持钥 open：密码吻合开锁
cmd /t suo xiang
clear
cmd /open
wait text 只听咔的一响 timeout=10000

# 钥匙上锁：同一把钥匙也能把锁扣上，锁态下 look 同样不渲染内容
# （与片首 C++ 锁同一遮蔽规则，回执为 key.lua do_use 私发）
clear
cmd /use yao shi on suo xiang
wait text 你用钥匙把锁锁上了 timeout=10000
clear
cmd /look
wait notext 里面有： timeout=3000

# 钥匙开锁：锁扣解除后内容菜单才渲染，取书步骤原样成立
clear
cmd /use yao shi on suo xiang
wait text 你用钥匙把锁打开了 timeout=10000

# 开锁后 look 渲染内容 → 取出秘籍（状态断言：背包真多这本书）
clear
cmd /look
wait menu
menu 剑法秘籍
wait item 剑法秘籍 timeout=10000
cmd /t 0

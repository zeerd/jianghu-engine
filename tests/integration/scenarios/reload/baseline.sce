# baseline.sce — 全 A 版基线：房/物/NPC 三路甲标记各就各位
#
# 判定：
#   · look 三句都走私发（room:say(msg, me)），只有本人收得到；
#   · 开箱取物断言 `wait item`（背包真多出东西），选中菜单不算数；
#   · use 断言本人版 message_vision（$N 对自己渲染「你…」）；
#   · 询问应答走私发（set_inquiry → say(answer, me)）。
# 三个标记串是夹具专用文本，别的账号发不出同样的句子（§8.4②）。

cmd /t 0
clear
cmd /look
wait text 【房景甲】 timeout=10000
wait text 【长描述甲】 timeout=10000
wait text 【共甲】 timeout=10000
wait object 重载测试员 timeout=10000

# 开箱取重载药：look 菜单 → 选中 → 背包断言
cmd /t zai xiang
clear
cmd /look
wait menu
menu 重载药
wait item 重载药 timeout=10000
cmd /t 0

# 使用 → 本人版回执含物品甲标记
cmd /t e2e_reload_item
clear
cmd /use
wait text 【物品甲】 timeout=10000
cmd /t 0

# 询问 → 私发应答含 NPC 甲标记
cmd /t e2e_reload_npc
clear
cmd /ask 重载
wait text 【NPC甲】 timeout=10000
cmd /t 0

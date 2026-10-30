# apprentice.sce — 半山腰：观察场景并拜师学艺
#
# 覆盖：CCmdStandard::apprentice → CLuaNpcT::attempt_apprentice 收徒，
#   （learn 命令要求 is_apprentice_of，故请教前必须先拜师）。
# 判定：房间描述私发断言；收徒应答「青云门的弟子」经 say(msg, me) 私发，
#   同房间别的账号收不到，也不会被收徒时的房间版磕头播报误满足。

cmd /t 0
wait room 青云山山脚
cmd /go 青云山半山腰
wait room 青云山半山腰
wait object 青云子

clear
cmd /look
wait text 这里是青云山的半山腰 timeout=10000

cmd /t qing yun zi
clear
cmd /apprentice
wait text 青云门的弟子 timeout=10000
cmd /t 0

# attr_register_verify.sce — 注册时输入的四维真的落到了 str/int/con/dex
#
# 依赖变量（run-integration-tests.ps1 注册阶段按 attrs 拆写进 --vars）：
#   attr_str / attr_int / attr_con / attr_dex : 注册输入的四项天赋
#
# 排在 common/login.sce 之后：登录即 flush_my_info，RTNINFO 的 E/F/G/H 携带
# attr() 合成值（dbase + apply/* + 技能/10）；此刻新号无装备无技能，合成值
# 即注册落库值。均值账号（21 21 21 21）拦「注册整体没写入」，非均值账号
# （四值互异，见 $Lanes L1 注释）拦「键位落错」——str↔dex 互换只有互异值
# 才测得出来。
#
# 判定：attr 条件只读本道角色的客户端快照（RTNINFO 按 object_id 过滤），
# 四条断言各钉一项，任一键位落错即红。

wait attr str == $attr_str timeout=10000
wait attr int == $attr_int timeout=10000
wait attr con == $attr_con timeout=10000
wait attr dex == $attr_dex timeout=10000

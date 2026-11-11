# register.sce — 注册一个新角色（游客 → 游戏大厅 → 注册专员分步注册）
#
# 依赖变量（由 run-integration-tests.ps1 写入 --vars 文件，或手工 --vars 提供）：
#   name   : 角色名（2~10 字节，需唯一）
#   pass   : 登录密码
#   email  : 邮箱
#   gender : 注册性别（男性/女性）——wakeup2 双人同睡按道声明异性
#            （$Lanes 的 Gender 字段，L5=男性 L6=女性），其余道默认男性
#   attrs: 天赋四维 str int con dex（单项 10~30、总和 84，register_clerk 校验）——
#          run-integration-tests.ps1 按 $Lanes 各道 Attrs 注入（缺省 21 21 21 21，
#          L1 取 24 20 21 19、L8 取 10 14 30 30），同时拆写 attr_str/attr_int/
#          attr_con/attr_dex 四键供登录后 common/attr_register_verify.sce 断言
#          四维真的落到了对应键上
# 产出变量：
#   acct : 注册成功后服务器返回的角色账号 id（供各道 lane_*.sce 登录使用）
#
# 说明：注册走「与 NPC 交谈 → 菜单回灌命令」的分步流程，与引擎
# lua-default/npc/register_clerk.lua 的 do_talk/reg_menu 一致。

login guest guest
wait login
wait room 游戏大厅
wait object 注册专员

cmd /t zhu ce
cmd /talk
wait menu
menu 与注册专员谈

wait menu
menu 设置角色姓名
menuinput $name

wait menu
menu 设置登录密码
menuinput $pass

wait menu
menu 设置邮箱地址
menuinput $email

wait menu
menu 选择性别
wait menu
menu $gender

wait menu
menu 设置天赋属性
menuinput $attrs

wait menu
menu 确认创建角色

wait text 注册成功
capture 账号是 as acct

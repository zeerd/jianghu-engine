# account.sce — 账号类命令一组：save / mudage / nickname / set·unset /
#   refuse 四分支 / passwd·verifypass·validate 改密验证链 / skill 拒绝分支 / who
#
# 位置：provision/social_supplies 之后（L4 仍在青云山山脚，老李头在场）。
# ⚠ 顺序纪律（时序竞态修复，run2 红定因）：DB 密集的改密链（passwd×4、
#   verifypass×2、validate×2 的 verify_user/verify_password 查询）必须聚在片段
#   前段（约 20~35s）执行——那是全道唯一没有任何道处于「heal 后 busy 命令」
#   边界的窗口。L2 的 learn_basic 首个 heal 边界约 48s、L3 的 levelup_heal
#   约 150s，DB 写晚一步都可能撞上它们的心跳窗口把 busy 撑过 sleep。
#   mudage/nickname/set/unset/refuse/skill/who 均无 DB 写，可安全放后段。
#
# ⚠ wire 纪律（§8.5）：CLI 在无 /t 目标时自动补 "0" 占位，服务端
#   「命令 目标 参数…」切分后 arg=目标之后的全部，所以本片段的多参数命令
#   （password/validate/set/refuse…）都不需要 /t；只有 skill 的拒绝
#   分支需要真目标（老李头），用完立即 /t 0。
#
# 改密链的状态依赖（顺序不可乱，源码 CmdUserAccount.cpp passwd/verify_pass/validate）：
#   · passwd 第一段要求 verify_password(id, 暗码)——新号暗码为空，故单参
#     `/password $pass` 即过；**必须在 verifypass 设暗码之前**，否则
#     空暗码校验不再匹配；
#   · verifypass 同理单参走「空暗码校验」，之后暗码才被设置；
#   · validate 失败分支要放在成功分支前（validate 成功会设 temp，之后再调直接跳过）。
#
# 判定：全部回执为 tell_object/notify_fail 私发，天然免疫同房串扰；
#   who 的回执是协议级 USERLIST（CLI 只填充 online_users_、无文本行），
#   故断言取「不出现未知命令/失败回执」——能证明命令已被服务端注册识别。

# ---------------------------------------------------------------------------
# —— 前段：DB 密集区（见头注释时序纪律）——
# save（30 秒冷却由 last_save_time 控，本道此前未 /save 过）
# ---------------------------------------------------------------------------
clear
cmd /save
wait text 档案储存完毕 timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# 改密链（顺序即状态依赖，见头注释）
# ---------------------------------------------------------------------------
# 旧密码错 → verify_user 失败
clear
cmd /password wrongpass e2enewpass9
wait text 与原密码不符，不能设置新密码 timeout=10000
sleep 400

# 旧密码对但暗码不匹配（新号暗码为空）→ verify_password 失败
clear
cmd /password $pass wrongverify
wait text 身份验证未通过 timeout=10000
sleep 400

# 两段式第一段：单参走空暗码校验通过
clear
cmd /password $pass
wait text 旧密码检测通过 timeout=10000
sleep 400

# 第二段：设新密码（≥5 字符）
clear
cmd /password e2enewpass9
wait text 修改密码成功 timeout=10000
sleep 400

# verifypass 失败分支：暗码太短
clear
cmd /verifypass ab
wait text 暗码不能少于5个字元 timeout=10000
sleep 400

# verifypass 成功：单参设暗码
clear
cmd /verifypass e2esecret1
wait text 修改暗码成功 timeout=10000
sleep 400

# validate 失败分支：密码错 / 暗码错
# ⚠ 密码已在上面改成 e2enewpass9，$pass 变量已过期——
#   暗码失败分支必须用新密码，否则先撞「密码不符」而不是「暗码不符」
clear
cmd /validate wrongpass e2esecret1
wait text 密码不符，不能进行进一步的命令操作 timeout=10000
sleep 400

clear
cmd /validate e2enewpass9 wrongsecret
wait text 暗码不符，不能进行进一步的命令操作 timeout=10000
sleep 400

# validate 成功（密码已是 e2enewpass9）
clear
cmd /validate e2enewpass9 e2esecret1
wait text 身份验证通过 timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# —— 后段：无 DB 写区（安全放后，见头注释时序纪律）——
# mudage：mud_age=0 时无时长后缀，断言取前缀；回执含动态 MAIN_NAME（金剑江湖）
# ---------------------------------------------------------------------------
clear
cmd /mudage
wait text 你已经为金剑江湖奋斗 timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# nickname：4 个中文字（GBK 8 字节）落在 check_legal_name 的 2~12 字节窗口内
# ---------------------------------------------------------------------------
clear
cmd /nickname 金剑大侠
wait text 你改变绰号为“金剑大侠” timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# set / unset 配对：先设 wimpy 再清掉（unset 只回 Ok.，靠 clear 隔离唯一）
# ---------------------------------------------------------------------------
clear
cmd /set wimpy 30
wait text 你设置环境变量：wimpy 为 30 timeout=10000
sleep 400

clear
cmd /unset wimpy
wait text Ok. timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# refuse 四分支：无设置 → 关一条 → 列表 → none 恢复
# ---------------------------------------------------------------------------
clear
cmd /refuse
wait text 你目前没有设置任何信息通道 timeout=10000
sleep 400

clear
cmd /refuse practice
wait text 关闭了practice的信息通道 timeout=10000
sleep 400

clear
cmd /refuse
wait text 你目前已经关闭的信息通道有 timeout=10000
sleep 400

clear
cmd /refuse none
wait text 信息通道恢复正常 timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# skill 拒绝分支：老李头非师父/非亲属，is_relative=0 且 recognize_apprentice=0
# ⚠ /t 用完立即复位
# ---------------------------------------------------------------------------
cmd /t 老李头
clear
cmd /skill
wait text 你无权查看对方技能 timeout=10000
sleep 400
cmd /t 0

# ---------------------------------------------------------------------------
# who：协议级 USERLIST，CLI 无文本渲染 → 断言「命令被识别且无失败回执」
# ---------------------------------------------------------------------------
clear
cmd /who
wait notext 未知的命令 timeout=3000
sleep 400

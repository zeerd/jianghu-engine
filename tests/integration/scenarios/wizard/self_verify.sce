# self_verify.sce — 巫师道首自验：命令链通路 + 系统参数探针 + 切磋被拒支
#
# 业务：巫师账号上线先自证三件事——
#   ① `/full` 顶满状态（房间版回执「金光乍现」）：授权 → 目标解析 → 执行三环通路；
#   ② `/systemparam` 全键循环 set（数值）→ setstr（字符串）→ query → querystr
#      → delete 收尾——只用全新探针键 e2e_probe；ip_limit/login_limit/kick_idle/
#      no_check_house/debug/log_level 是保留键，改它们会真的动运行时行为，禁碰；
#   ③ `/fight` 对山脚向导：巫师绕房间 no_fight，friendly NPC 拒不应战（私发回执）。
#
# 判定：②③全为私发回执，他道无从串扰；②③两块**不 clear**（§8.6⑦：clear 落在服务
#   请求可能到达的窗口会把请求吞掉且永不重发——先到的请求由下一片段的服务段 wait
#   从缓冲消费）。③ 的房间版挑战语含「领教」，不进本块断言——L5/L6 切磋段在等该词；
#   同房对象先 wait object 再定向（§8.5）。
#
# 放置：lane_wizard 的第二个 include（login 之后、service 之前）——服务段的固定
#   wait 顺序自下一片段才开始，道首三步不占任何信号窗口。
# 目标纪律：①自指 `/t 1`、③`/t lao li`，两块各自块末 /t 0 收干净。

# 自验：巫师命令链通路（grant 100 → get_room_char/find_player → full）。
# /t 1 指向自己（巫师账号数字 ID 固定 1），full 顶满 hp/eff_hp/mp/eff_mp，
# 回执为房间版播报「只见巫师一号浑身金光乍现…」——用它确认命令被服务端执行。
cmd /t 1
cmd /full
wait text 金光乍现
assert attr hp >= 1
cmd /t 0

# --- 系统参数自检（全新探针键 e2e_probe，收尾删除，不碰任何保留键） -----------
# 键名全服唯一且读写回执都走私发，故本块不 clear（§8.6⑦）——先到的请求照样被
# 下面的服务段 wait 从缓冲里消费。
#   数值支 → 字符串支 → 数值读 → 字符串读 → 删除收尾（读回执都带键值，
#   删除后再读就是缺省值，故读必须排在删除之前）
cmd /systemparam set e2e_probe 1
wait text e2e_probe为1 timeout=10000
cmd /systemparam setstr e2e_probe abc
wait text e2e_probe为abc timeout=10000
cmd /systemparam query e2e_probe
wait text e2e_probe=1 timeout=10000
cmd /systemparam querystr e2e_probe
wait text e2e_probe=abc timeout=10000
cmd /systemparam delete e2e_probe
wait text 删除系统参数e2e_probe timeout=10000

# --- 切磋邀请被友好 NPC 拒绝（本道起始房间即山脚，向导在场） -----------------
# 房间带 no_fight，但只拦非巫师（本道账号是巫师）；向导 attitude=friendly、
# 气血满 → 拒不应战并私发「并不想跟你较量」，不进比试、不进战斗。
wait object 老李头 timeout=10000
cmd /t lao li
cmd /fight
wait text 并不想跟你较量 timeout=10000
cmd /t 0

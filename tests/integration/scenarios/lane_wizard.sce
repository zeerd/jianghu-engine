# lane_wizard.sce — L9 道：巫师服务（为有特别战斗需求的道改定属性/技能）
#
# 账号机制：巫师一号（cc_id=1）随服务端启动自举（Sql.cpp ensure_wizard_accounts，
#   grant=100），不走注册流程——run-integration-tests.ps1 对本道 NoRegister，
#   vars 直写 acct=1 + pass（config [wizard] password，缺省 12345678）。
#   首登 cc_savedata 为空 → verify_user 新人分支 init_new_char 建档，
#   age/start_city 取 config（start_age=16 / start_room=青云山山脚）。
#
# 服务协议（§2.6）：需求道在服务点 `/t 1` + `tell` 请求 → 本道按固定顺序
#   `wait text` 收请求 → 执行服务段 → 私发回执；需求道 wait 回执或直接状态断言。
#   本道服务段之间禁止 clear（§8.6⑦）——细则见 service.sce 片段头注。
#
# 稀缺资源归属（§8.4③）：本道不采购、不占用任何共享世界资源；巫师命令只
#   改定目标账号的 dbase/技能，不动物品。
#
# 片段链：登录 → 道首自验（/full 通路 + 系统参数探针 + 切磋被拒支）→
#   服务链七路供能 + 链尾房间重整。本道全程不移动（服务跨房 /t 定向），
#   链尾才第一次换房。

include common/login.sce
include wizard/self_verify.sce
include wizard/service.sce

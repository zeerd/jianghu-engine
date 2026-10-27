# login.sce — 各道首个片段：登录并确认落在起始房间
#
# 前置变量（由 run-integration-tests.ps1 注册阶段经 --vars 注入，每道独立一份）：
#   acct : 角色账号 id（register.sce 注册后捕获）
#   pass : 登录密码
#
# 说明：全新注册角色首次登录落在 config.ini 的 start_room（青云山山脚）。
# 各道（lane）之间互不共享角色状态，故每道都从这里开始。

login $acct $pass
wait login
wait room 青云山山脚
sleep 800

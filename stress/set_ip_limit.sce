# set_ip_limit.sce — 运行期设置 [server] ip_limit（不重启服务器）。
#
# 生产服务器按启动期快照读 ip_limit（CombatConfig::load_ini），改
# config.ini 后要重启才生效；本剧本走巫师命令 systemparam 运行期改值，
# 立即生效但不落盘——config.ini 里的值仍是重启后的基准，两处保持一致。
#
# 用法：
#   jianghu_cli.exe --config stress\cli.conf --script stress\set_ip_limit.sce \
#       --vars stress\tmp\ip_limit.vars
# vars 内容一行：value=300
wait always
login 2 1qaz*IK<
wait login
cmd /systemparam set ip_limit $value
wait text 设定系统参数

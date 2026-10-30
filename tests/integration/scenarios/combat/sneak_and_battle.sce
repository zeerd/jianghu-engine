# sneak_and_battle.sce — 第二轮打坐攒足当前内力 → 投掷偷袭山鸡 → 收口成战斗
#
# 覆盖：
#   CCmdStandard::exercise 的长程打坐：`set exercise 120` 花的是 120 点练习预算，
#     逐拍把 1 气换成 1 内力、当前 mp 累加 120；**内力上限不动**（收工时只有
#     「mp ≥ max_mp×2」才抬 1 点上限），经验果给的 101 上限原样保留，
#     累加出的 121 < 2×101 所以也不会被压回——正好停在 do_sneak 的门槛之上。
#   lua-default/skill/basic/throwing 的 do_sneak：先找暗器（背包里的棋子）、
#     再看内力（mp≥100 才放行，否则回「你的法力不足」）、命中/落空两分支
#   战斗结算（wait battle / battle_end）
#
# 判定：
#   · 打坐的「运功完毕」走 message_channel，本人版是「你运功完毕」、房间版是
#     「<名字>运功完毕」，只认本人版；
#   · `wait attr mp >= 100` 断的是**当前内力**（不是上限），直接证明经验果把上限
#     抬到 101 之后、这轮打坐确实把当前内力攒过了偷袭门槛；
#   · 偷袭的「准备偷袭山鸡」由 tell_object 私发；命中「中暗器了」、落空
#     「躲过了你的偷袭」也只发给本人/本房，而山顶只有本道。
#
# 说明：`/cast` 的目标只由 `/t` 给（cast <技能> <法术>），命令行里不能再带
#   目标名——analyse_string 按第一个空格切分，多出来的名字会被并进法术名，
#   find_magic 落空，服务端回「你所使用的技能不支持该法术。」

# ---------------------------------------------------------------------------
# 打坐：经验果已把上限抬到 101（当前内力仍是 1），这轮只把当前内力攒到 ≥100
# ---------------------------------------------------------------------------
clear
cmd /set exercise 120
wait text 你设置环境变量 timeout=10000

clear
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
wait text 你运功完毕 timeout=180000
wait attr mp >= 100 timeout=10000

# 打坐耗了 120 气，开打前补满
repeat 4
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

# ---------------------------------------------------------------------------
# 投掷实战：命中走「非战斗」分支只结仇恨不掉血，落空走「侧身躲过」分支；
# 两种结局都可能开战，故 sleep 等 busy 清空后统一用 kill 收口——已在战斗中时
# kill 是冗余重申，未开战时它补上这一战，两条路径都收敛到 battle → battle_end
# ---------------------------------------------------------------------------
wait object 山鸡
cmd /t shan ji
clear
cmd /cast throwing sneak
wait text 准备偷袭山鸡 timeout=10000
wait text 中暗器了|躲过了你的偷袭 timeout=10000
sleep 3000
cmd /kill
wait battle
wait battle_end timeout=180000
cmd .info
cmd /t 0

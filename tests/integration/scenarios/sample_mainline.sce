# sample_mainline.sce — lua-sample 主线单人剧本（无头 E2E）
#
# 前置变量（由 run-integration-tests.ps1 注册阶段经 --vars 注入）：
#   acct : 角色账号 id（register.sce 注册后捕获）
#   pass : 登录密码
#
# 覆盖的引擎能力（按 lua-sample 示例组织）：
#   登录 / 房间 do_look / 向导问答 / 内置菜单 / 容器取物 / 商人买卖 / 装备
#   茶棚液体装填与饮用 / 拜师 / 学艺（基础+高级武功链路）/ 读书 / 升级 /
#   任务（通用挂点 pre_talk / pre_accept_object，见 data/quest.lua）/
#   投掷（学艺 → valid_learn 两道门槛 → enable → 树箱经验果拓宽经脉 →
#   打坐把内力练进去 → 偷袭实战）/
#   打坐机制（max_mp 0 → 1）/ 睡觉恢复 / 战斗 / 尸体腐烂（CCorpse::decay，
#   120 心跳）/ 随机树箱 / 副本进出
#   生活类段落（打坐恢复、毒药补药、传送符、观察人物、钱庄、社交表情、
#   频道禁词与频道分组）在 sample_social.sce，跑在本剧本之后（同一角色连续登录）。
#
# 说明：剧本按「全新角色首次登录落在青云山山脚」编写；每个 run 使用独立测试库
# 与全新注册角色，保证房间/箱子/随机事件为初始状态。
# 主线收尾于青云山山顶（副本出口），供 sample_social.sce 接续。

login $acct $pass
wait login
wait room 青云山山脚
sleep 800

# ---------------------------------------------------------------------------
# 房间观察：触发房间 do_look（common.say_long + SendMenu）
# ---------------------------------------------------------------------------
cmd /t 0
cmd /look
sleep 500

# ---------------------------------------------------------------------------
# 向导问路：set_inquiry 文本问答 + set_inquiry_func 回调（发放 50000 经验）
# + 排行榜查询（paihangbang：查询成功且无人达标时脚本按 0 条播报）
# ---------------------------------------------------------------------------
wait object 老李头
cmd /t 老李头
cmd /ask 山
sleep 500
cmd /ask 师父
sleep 500
cmd /ask 排行榜
wait text 排行榜上还没有人达到上榜资格 timeout=10000
cmd /t 0

# ---------------------------------------------------------------------------
# 开箱取物：对容器 look 弹出内容菜单，选中后走 get ... from ...
# ---------------------------------------------------------------------------
cmd /t 木箱
cmd /look
wait menu
menu 铁剑
sleep 600
cmd /t 木箱
cmd /look
wait menu
menu 金币
sleep 600
cmd /t 药箱
cmd /look
wait menu
menu 金创药
sleep 600

# ---------------------------------------------------------------------------
# 商人：货物清单 / 购买 / 出售（CVendor::do_list / do_buy / do_sell）
# 回春丹（商品 6）买 44 粒：主线自身消耗约 33 粒（含投掷段学艺续气 15 粒——
# 每次 learn 扣 100 气，学艺按「补满 → 学 3 次」循环，经验果之后补气 3 粒），
# sample_social.sce 的毒药/补药对照要按「毒伤 ÷ 每粒 100 点」反复服用到补满，
# 再留一粒验证「不需要」拒绝分支——药量随样本毒伤走，样本改数值时这里要跟着改。
# 金创药（商品 5）买 7 包：主线服用 2 包，任务交物扣 1 包，social 与
# sample_house.sce 各要留 1 包（一个服用、一个验证拒收）。
# 棋子（商品 10）买 1 盒：投掷用暗器（skill_type=throwing），投掷段要 wield 它。
# ---------------------------------------------------------------------------
cmd /t 杂货老板
cmd /list
sleep 600
cmd /buy 1
sleep 600
repeat 7
cmd /buy 5
sleep 600
end
repeat 44
cmd /buy 6
sleep 600
end
cmd /buy 7
sleep 600
cmd /buy 8
sleep 600
cmd /buy 9
sleep 600
cmd /buy 10
sleep 600
cmd /sell 金创药
sleep 600

# ---------------------------------------------------------------------------
# 任务（通用挂点 hooks.pre_talk / hooks.pre_accept_object，见 data/quest.lua）：
#   没接委托就交物 → 交物钩子不接管，引擎默认拒收
#   接委托 → 交物被钩子接管（物品真的移交给老李头）→ 复命领赏
#   任务状态记在角色 dbase（quest/lilaotou）；每个 run 都是新注册角色，无残留
# ---------------------------------------------------------------------------
cmd /t 老李头
clear
cmd /give 金创药
wait text 对方不要你的东西 timeout=10000
sleep 600
clear
cmd /ask 委托
wait text 买一粒金创药来 timeout=10000
sleep 600
clear
cmd /give 金创药
wait text 接过金创药 timeout=10000
sleep 600
clear
cmd /ask 复命
wait text 老朽的一点心意 timeout=10000
sleep 600
cmd /t 0

# ---------------------------------------------------------------------------
# 引擎标准询问流程的商家回退（CLuaNpcT::base_do_talk → CNpc::do_talk_inquiry）：
# 杂货老板是 CVendor（type="vendor"），脚本没登记任何询问条目 → 询问流程直接
# 回绝。老李头这类非商人的 NPC 走的是脚本侧 do_lua_inquiry，两条路互不重叠
# ---------------------------------------------------------------------------
cmd /t 杂货老板
clear
cmd /ask 商人
wait text 对方并不愿意理你 timeout=10000
sleep 600
cmd /t 0

# ---------------------------------------------------------------------------
# 装备武器：wield
# ---------------------------------------------------------------------------
cmd /t 铁剑
cmd /wield
sleep 600
cmd .info

# ---------------------------------------------------------------------------
# 茶棚：fill 装水 + use 饮用（resource/water + liquid_base）
# ---------------------------------------------------------------------------
cmd /t 0
cmd /go 山脚茶棚
wait room 山脚茶棚
sleep 500
cmd /look
sleep 400
cmd /t 茶壶
cmd /fill
sleep 600
cmd /t 茶壶
cmd /use
sleep 600
cmd /t 茶壶
cmd /look
sleep 400

# ---------------------------------------------------------------------------
# 半山腰：拜师学艺 / 高级武功 valid_learn 门槛 / 学艺 / 读书与打坐的
# 错误分支（先学武，再上山挑战）
# ---------------------------------------------------------------------------
cmd /t 0
cmd /go 青云山山脚
wait room 青云山山脚
cmd /go 青云山半山腰
wait room 青云山半山腰
wait object 青云子
cmd /look
sleep 400
cmd /t 青云子
clear
cmd /apprentice
wait text 青云门的弟子 timeout=10000
sleep 600
clear
cmd /learn force_adv
wait text 基本内功火候不够 timeout=10000
sleep 600
clear
cmd /learn sword_adv
wait text 基本剑法火候不够 timeout=10000
sleep 600
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
clear
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
clear
cmd /learn literate
wait text 进步了 timeout=10000
sleep 600
cmd .skills

# study/enable/prepare/practice 均为错误分支：断言各自确定的失败文案，
# 行为漂移会立刻红（clear 隔离，避免旧输出误满足等待）
cmd /t 内功秘籍
clear
cmd /study
wait text 你要读什么？ timeout=10000
sleep 600
cmd /t 0
clear
cmd /enable force
wait text 指令格式：enable timeout=10000
sleep 600
clear
cmd /prepare unarmed
wait text 没有使用任何有效特殊技能 timeout=10000
sleep 600
clear
cmd /practice force
wait text 只能练习用 enable timeout=10000
sleep 600
cmd .skills

# ---------------------------------------------------------------------------
# 高级武功链路（睡醒满血为前提）：learn 越过 valid_learn 门槛 →
#   enable 装备高级技能 → /enable 列表断言映射 →
#   practice 被技能脚本拒绝（force_adv 只能请教 / sword_adv 内力不足）→
#   cast 自疗术走 mp=0 施法失败分支（法术由 force_adv 的 add_magic 声明）
# ---------------------------------------------------------------------------
repeat 2
cmd /upgrade
wait text 升级成功 timeout=10000
clear
end
repeat 2
cmd /t 金创药
clear
cmd /use
sleep 3200
end
cmd /t 0
cmd /sleep
wait text 开始睡觉 timeout=5000
wait text 一觉醒来 timeout=120000
clear
cmd .info
clear
cmd /t 青云子
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
clear
cmd /t 青云子
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
clear
cmd /t 青云子
repeat 3
clear
cmd /learn force
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
clear
cmd /t 青云子
clear
cmd /learn force_adv
wait text 进步了 timeout=10000
sleep 600
repeat 2
clear
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
clear
cmd /t 青云子
repeat 3
clear
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
clear
cmd /t 青云子
repeat 3
clear
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
end
clear
repeat 4
cmd /t 回春丹
clear
cmd /use
sleep 3200
end
repeat 2
clear
cmd /t 青云子
cmd /learn sword
wait text 进步了 timeout=10000
sleep 600
end
clear
cmd /learn sword_adv
wait text 进步了 timeout=10000
sleep 600
clear
cmd /enable force force_adv
wait text 内力必须重新修炼 timeout=10000
sleep 600
clear
cmd /enable sword sword_adv
wait text Ok. timeout=10000
sleep 600
clear
cmd /enable
wait text 以下是你目前使用中的特殊技能 timeout=10000
wait text sword：sword_adv timeout=10000
wait text force：force_adv timeout=10000
sleep 600
clear
cmd /practice force
wait text 只能用请教的来增加熟练度 timeout=10000
sleep 600
clear
cmd /practice sword
wait text 体力或内力不够 timeout=10000
sleep 600
clear
cmd /cast force heal
wait text 内力不足，无法施展自疗术 timeout=10000
sleep 600

# ---------------------------------------------------------------------------
# 投掷链路（lua-default/skill/basic/throwing + lua-sample/skill/throw_adv）：
#   ① 未学基本投掷就学高级投掷 → valid_learn 门槛①「火候不够」
#   ② 学满 12 次基本投掷（improve_skill 每次恰好 +1 级）后，手里还握着铁剑
#      → valid_learn 门槛②「必须手持暗器」
#   ③ 铁剑插回剑鞘 → 换上棋子（skill_type=throwing）→ 学会高级投掷 →
#      enable throwing throw_adv（「throwing」是基础种类，不能自映射，
#      必须有高级技能承接，否则 cast 报「请先准备技能」）
#   learn 每次扣 100 气且 hp<100 拒学，沿用主线的「补满 → 学 3 次」循环；
#   每次 heal 都断言生效或被拒，避免「没吃上药却继续学」的隐性失败
# ---------------------------------------------------------------------------
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 青云子
clear
cmd /learn throw_adv
wait text 基本投掷火候不够 timeout=10000
sleep 600
repeat 4
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 青云子
repeat 4
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 青云子
repeat 4
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 青云子
repeat 4
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end
clear
cmd /learn throw_adv
wait text 必须手持暗器类武器 timeout=10000
sleep 600

# 换暗器上手：铁剑回鞘 → 棋子（wield_msg 已断言，确认 wield 真的成功）
clear
cmd /unwield all
wait text 插回剑鞘 timeout=10000
sleep 600
cmd /t 棋子
clear
cmd /wield
wait text 从怀中夹出一枚棋子 timeout=10000
sleep 600
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end
cmd /t 青云子
clear
cmd /learn throw_adv
wait text 进步了 timeout=10000
sleep 600
clear
cmd /enable throwing throw_adv
wait text Ok. timeout=10000
sleep 600

# ---------------------------------------------------------------------------
# 打坐（exercise）：enable force force_adv 已把内力清零（mp=0），玩家角色的
# 内力上限 max_mp 也是 0（引擎只为 NPC 推导 max_mp，玩家的归打坐去涨）。
# 这里只测打坐的机制本身：一轮收工时若「攒到的内力 ≥ max_mp×2」就把 max_mp
# 抬 1 点、并把内力设成新的上限——于是 max_mp 0 → 1，
# 「你的内力增加了！！」正是这条分支的直接回执。
# （把内力攒到能放偷袭的量级是另一回事：等经验果拓宽经脉之后再练，见山顶段）
# ---------------------------------------------------------------------------
clear
cmd /set exercise 10
wait text 你设置环境变量 timeout=10000
sleep 400
clear
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
wait text 运功完毕 timeout=60000
wait text 内力增加了 timeout=10000
wait attr mp >= 1 timeout=10000
sleep 400

# ---------------------------------------------------------------------------
# 上山顶：**先走怪树分支**（树箱 → 经验果拓宽经脉）→ 把内力练进去 → 打山鸡
#
# 顺序为什么是这样：经验果只把内力上限 +100，当前内力仍停在 1——引擎把当前
# 内力压在 max_mp 之下，上限抬了内力不会自己跟上来，所以必须再打坐一轮把
# 内力真练进去：max_mp=101、打坐 120 点 → 收工时内力 121 < 2×101 不会被压回，
# 正好停在 do_sneak 要的「mp ≥ 100」之上。
# ---------------------------------------------------------------------------
cmd /go 青云山山顶
wait room 青云山山顶
sleep 800
cmd /look
sleep 500

# ---------------------------------------------------------------------------
# 随机树箱：概率性刷新的容器（sample_guide.do_refresh + Add_Box），开出一枚经验果
# ---------------------------------------------------------------------------
wait object 一个奇怪的树 timeout=240000
cmd /t 0
cmd /t 一个奇怪的树
cmd /look
wait menu
menu 经验果
sleep 800

# 经验果：+100000 历练、内力上限 +100（sample_xp.lua；吃药 busy 2 拍）
cmd /t 经验果
clear
cmd /use
wait text 吃下一粒经验果 timeout=10000
sleep 3000
cmd .info

# 第二轮打坐：把内力练进新上限里（这轮不测 0→1，只断言内力攒够）
clear
cmd /set exercise 120
wait text 你设置环境变量 timeout=10000
sleep 400
clear
cmd /exercise
wait text 你坐下来运气用功 timeout=10000
wait text 运功完毕 timeout=180000
wait attr mp >= 100 timeout=10000
sleep 400

# 打坐耗了 120 气，开打前补满（回春丹每粒 busy 2 拍，粒与粒之间留 3.2 秒）
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 精神立刻好了许多|不需要用回春丹 timeout=10000
sleep 3200
end

# ---------------------------------------------------------------------------
# 投掷实战：do_sneak 先找暗器（get_first_throwing_weapon）、再看内力（mp ≥ 100）。
# 命中走「非战斗」分支——只结仇恨不掉血，未命中走「侧身躲过」分支；两分支最后
# 都会掷出并消耗一枚棋子。两种结局都可能开战，故 sleep 等 busy 清空后统一用
# kill 收口：已在战斗中时 kill 是冗余重申，未开战时它补上这一战，两条路径都
# 收敛到「wait battle → wait battle_end」。
#
# 注意：/cast 的目标只由 /t 给（cast <id> <技能> <法术>），**命令行里不能再带
# 目标名**——analyse_string 按第一个空格切分，多出来的名字会被并进法术名，
# find_magic 落空，服务端回「你所使用的技能不支持该法术。」
# ---------------------------------------------------------------------------
wait object 山鸡
cmd /t 山鸡
sleep 500
clear
cmd /cast throwing sneak
wait text 准备偷袭山鸡 timeout=10000
wait text 中暗器了|躲过了你的偷袭 timeout=10000
sleep 3000
cmd /kill
wait battle
wait battle_end timeout=180000
sleep 1000
cmd .info

# ---------------------------------------------------------------------------
# 尸体腐烂（CCorpse::decay）：遇害者尸体带 120 拍心跳，服务端 1 拍 = 1 秒，
# 到点后播报第一段腐烂文案——播报只发到尸体所在房间，所以必须留在山顶等它
# （树箱已在前面开过，这一段就是干等）
# ---------------------------------------------------------------------------
wait text 开始腐烂了 timeout=180000

# ---------------------------------------------------------------------------
# 副本：闯入山寨 → 见副本管理员 → 离开副本
# ---------------------------------------------------------------------------
cmd /t 0
cmd /look
wait menu
menu 闯入山寨
wait room 黑风寨庭院
sleep 1000
cmd .look
cmd /t 0
cmd /go 聚义大厅
wait room 黑风寨大厅
sleep 1000
cmd .look
wait object 小头目
cmd /t 黑风寨小头目
cmd /talk
wait menu
menu 离开副本
wait room 青云山山顶
sleep 800

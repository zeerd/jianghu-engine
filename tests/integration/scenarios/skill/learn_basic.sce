# learn_basic.sce — 半山腰：拜师学艺 + 基础技能请教 + 四条命令错误分支
#
# 覆盖：
#   CmdStandardMaster::apprentice / attempt_apprentice 收徒
#   LuaSkill::valid_learn 的两道高级门槛（基本内功/基本剑法火候不够）
#   CmdStandardMaster::learn 的请教链路（improve_skill 逐级 +1）
#   study / enable / prepare / practice 的错误分支（各断言自身确定的失败文案，
#   行为漂移会立刻红；clear 隔离，避免旧输出误满足等待）
#
# 并行说明：
#   · 收徒应答 say(msg, me) 私发，「青云门的弟子」只有本账号收得到；
#     收徒时的磕头播报虽是房间版，但文案不含「青云门的弟子」，不会误满足。
#   · 请教进度「进步了」由 tell_object 私发（CharSkill.cpp / learn 内），
#     同房间其他道的请教不会串进来。
#   · 「火候不够」「你要读什么」等一律 notify_fail，仅本人可见。

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

# 高级技能的 valid_learn 门槛：基本火候不够 → 拒绝
clear
cmd /learn force_adv
wait text 基本内功火候不够 timeout=10000
clear
cmd /learn sword_adv
wait text 基本剑法火候不够 timeout=10000

# 基础技能请教（每次扣 100 气；learn 内部会立刻 flush_my_info 推给客户端，
# 所以这里的状态是最新值——比等几分钟后回来断言要稳）
clear
cmd /learn force
wait text 进步了 timeout=10000
clear
cmd /learn sword
wait text 进步了 timeout=10000
clear
cmd /learn literate
wait text 进步了 timeout=10000

# 三次请教共扣 300 气，此刻当前气血必然不满——这正是后面「睡觉补 hp」要对照的起点
# （对照值取升级后的 3 级上限 531，与 skill/levelup_rest.sce 睡醒断言配对）
wait attr hp < 531 timeout=10000
cmd .skills

# 气血已被三次请教扣到 0，而 study 要 hp > cost（force 没配 jing_cost，cost 触底为 5）
# 才会走「研读心得」分支——先补一粒回春丹再读；回执是本人专属双分支，不会被同房串台。
cmd /t hui chun dan
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200

# ---------------------------------------------------------------------------
# study 的两条分支 + enable/prepare/practice 的错误分支
#   study 的目标必须是「自己背包里的物件」（CmdStandardMaster::study 里
#   get_self_object 拿不到就回错）：拿师父做目标 → 「你要读什么？」；
#   再定向到买来的内功秘籍 → 真读进去，回执「你研读有关基本内功的技巧…」
#   由 tell_object 私发，且技能名固定为基本内功，可确定断言
# ---------------------------------------------------------------------------
cmd /t qing yun zi
clear
cmd /study
wait text 你要读什么？ timeout=10000

cmd /t sample_book_force
clear
cmd /study
wait text 你研读有关基本内功的技巧，似乎有点心得 timeout=10000
cmd /t 0
clear
cmd /enable force
wait text 指令格式：enable timeout=10000
clear
cmd /prepare unarmed
wait text 没有使用任何有效特殊技能 timeout=10000
clear
cmd /practice force
wait text 只能练习用 enable timeout=10000
cmd .skills
cmd /t 0

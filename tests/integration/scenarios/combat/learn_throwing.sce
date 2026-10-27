# learn_throwing.sce — 投掷链路：基本投掷 → 高级投掷的两道 valid_learn 门槛 → 激发
#
# 覆盖（lua-default/skill/basic/throwing + lua-sample/skill/throw_adv）：
#   ① 未学基本投掷就学高级投掷 → 门槛①「基本投掷火候不够」
#   ② 基本投掷够 10 级但空手 → 门槛②「必须手持暗器」（valid_learn 在 hp 检查
#      之前跑，所以只要武器条件不满足就一定回这条）
#   ③ 换上棋子（skill_type=throwing）→ 学会高级投掷 →
#      enable throwing throw_adv（cast 命令要求 query_skill_mapped("throwing")
#      非空，而基本技能不能自我映射，故必须有高级技能承接）
#
# 判定：请教进度「进步了」私发；补气本人版双分支；wield/wield 后的回执见各步。
# 说明：每次迭代先 clear，保证本轮等待只看本轮回执。

cmd /t 青云子

clear
cmd /learn throw_adv
wait text 基本投掷火候不够 timeout=10000
sleep 600

# 内功链刚把气耗去大半（每次请教扣 100 气），先补满再进投掷的请教循环——
# 不补的话这一组第 3 次请教会撞上 hp<100 的「你今天太累了，结果什么也没有学到」
repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
repeat 3
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
repeat 3
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
sleep 3200
end

cmd /t 青云子
repeat 4
clear
cmd /learn throwing
wait text 进步了 timeout=10000
sleep 600
end

# 空手时卡在武器条件上：门槛②
clear
cmd /learn throw_adv
wait text 必须手持暗器类武器 timeout=10000
sleep 600

# 换暗器上手
cmd /t 棋子
clear
cmd /wield
wait text 从怀中夹出一枚棋子 timeout=10000
sleep 600

repeat 4
cmd /t 回春丹
clear
cmd /use
wait text 你吃下一粒回春丹|你现在不需要用回春丹 timeout=10000
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
cmd /t 0

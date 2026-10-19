-- rank.lua — 称谓查询（江湖称号 / 尊称 / 蔑称 / 自称 / 近称 / 外貌）。
--
-- 钩子契约（hooks.rank_rule(phase, a, b)）：
--   "rank" / "respect" / "rude" / "self" / "self_rude"  → a=对象, b=nil
--   "close" / "self_close"                              → a=me, b=对方(可为 nil)
--   "waimao"                                            → a=性别字符串, b=per 数值
--   返回：称谓字符串（waimao 无匹配时返回空串）。
--
-- 对象参数为 jianghu_object userdata，字符串为 GBK 编码。

local M = {}

-- 男性 / 无性 外貌文案（per 11~39），越界由 _waimao_default 处理
local WAIMAO_MALE = {
    [39] = "现在神清气爽，骨格清奇，宛若仙人。",
    [38] = "现在丰神俊朗，长身玉立，宛如玉树临风。",
    [37] = "现在飘逸出尘，潇洒绝伦。",
    [36] = "现在面如美玉，粉妆玉琢，俊美不凡。",
    [35] = "现在丰神如玉，目似朗星，令人过目难忘。",
    [34] = "现在粉面朱唇，身姿俊俏，举止风流无限。",
    [33] = "现在双目如星，眉梢传情，所见者无不为之心动。",
    [32] = "现在举动如行云游水，独蕴风情，吸引所有异性目光。",
    [31] = "现在双眼光华莹润，透出摄人心魄的光芒。",
    [30] = "生得英俊潇洒，风流倜傥。",
    [29] = "生得目似点漆，高大挺俊，令人心动。",
    [28] = "生得面若秋月，儒雅斯文，举止适度。",
    [27] = "生得剑眉星目，英姿勃勃，仪表不凡。",
    [26] = "生得满面浓髯，环眼豹鼻，威风凛凛，让人倾倒。",
    [25] = "生得眉如双剑，眼如明星，英挺出众。",
    [24] = "生得虎背熊腰，壮健有力，英姿勃发。",
    [23] = "生得肤色白皙，红唇墨发，斯文有礼。",
    [22] = "生得浓眉大眼，高大挺拔，气宇轩昂。",
    [21] = "生得鼻直口方，线条分明，显出刚毅性格。",
    [20] = "生得眉目清秀，端正大方，一表人才。",
    [19] = "生得腰圆背厚，面阔口方，骨格不凡。",
    [18] = "生得相貌平平，不会给人留下什么印相。",
    [17] = "生得膀大腰圆，满脸横肉，恶形恶相。",
    [16] = "生得獐头鼠须，让人一看就不生好感。",
    [15] = "生得面颊深陷，瘦如枯骨，让人要发恶梦。",
    [14] = "生得肥头大耳，腹圆如鼓，手脚短粗，令人发笑。",
    [13] = "生得贼眉鼠眼，身高三尺，宛若猴状。",
    [12] = "生得面如桔皮，头肿如猪，让人不想再看第二眼。",
    [11] = "生得呲牙咧嘴，黑如锅底，奇丑无比。",
}

-- 女性外貌文案（per 11~39）
local WAIMAO_FEMALE = {
    [39] = "现在美若天仙，不沾一丝烟尘。",
    [38] = "现在灿若明霞，宝润如玉，恍如神妃仙子。",
    [37] = "现在气质美如兰，才华馥比山，令人见之忘俗。",
    [36] = "现在丰润嫩白，婴桃小口，眉目含情，仿佛太真重临。",
    [35] = "现在鲜艳妩媚，袅娜风流，柔媚姣俏，粉光脂艳。",
    [34] = "现在鬓若刀裁，眉如墨画，面如桃瓣，目若秋波。",
    [33] = "现在凤眼柳眉，粉面含春，丹唇贝齿，转盼多情。",
    [32] = "现在眉目如画，肌肤胜雪，真可谓闭月羞花。",
    [31] = "现在娇若春花，媚如秋月，真的能沉鱼落雁。。",
    [30] = "生得闲静如姣花照水，行动似弱柳扶风，体态万千。",
    [29] = "生得娇小玲珑，宛如飞燕再世，楚楚动人。",
    [28] = "生得鸭蛋秀脸，俊眼修眉，黑发如瀑，风情万种。",
    [27] = "生得削肩细腰，身材苗条，娇媚动人，顾盼神飞。",
    [26] = "生得丰胸细腰，妖娆多姿，让人一看就心跳不已。",
    [25] = "生得粉嫩白至，如芍药笼烟，雾里看花。",
    [24] = "生得腮凝新荔，目若秋水，千娇百媚。",
    [23] = "生得鲜艳妍媚，肌肤莹透，引人遐思。",
    [22] = "生得巧笑嫣然，宛约可人。",
    [21] = "生得如梨花带露，清新秀丽。",
    [20] = "生得风姿楚楚，明丽动人。",
    [19] = "生得肌肤微丰，雅淡温宛，清新可人。",
    [18] = "生得虽不标致，倒也白净，有些动人之处。",
    [17] = "生得身材瘦小，肌肤无光，两眼无神。",
    [16] = "生得干黄枯瘦，脸色腊黄，毫无女人味。",
    [15] = "生得满脸疙瘩，皮色粗黑，丑陋不堪。",
    [14] = "生得一嘴大暴牙，让人一看就没好感。",
    [13] = "生得眼小如豆，眉毛稀疏，手如猴爪，不成人样。",
    [12] = "生得八字眉，三角眼，鸡皮黄发，让人一见就想吐。",
    [11] = "生得歪鼻斜眼，脸色灰败，直如鬼怪一般。",
}

-- 江湖称号：按性别分派，读 weiwang / trade/score / class / repute / pks / thief
function M.rank(ob)
    local budd = 0
    if ob:querystr("family/family_name") == "少林派" then
        budd = ob:query_skill("buddhism", 1)
    end

    local shen = ob:query("repute")
    local cla = ob:querystr("class")
    local score = ob:query("trade/score")

    if ob:querystr("gender") == "女性" then
        if ob:query("weiwang") >= 100 then
            return "【绝世佳人】"
        end

        if score > 500 then
            if score < 2000 then
                return "【 小  贩 】"
            elseif score < 15000 then
                return "【 商  人 】"
            elseif score < 50000 then
                return "【大 商 人】"
            else
                return "【红顶商人】"
            end
        end

        if cla == "bonze" then
            return "【 尼  姑 】"
        elseif cla == "taoist" then
            return "【 女  冠 】"
        elseif cla == "bandit" then
            return "【 女飞贼 】"
        elseif cla == "dancer" then
            return "【 舞  姬 】"
        elseif cla == "scholar" then
            return "【 才  女 】"
        elseif cla == "officer" then
            return "【 女  官 】"
        elseif cla == "fighter" then
            return "【 女武者 】"
        elseif cla == "swordsman" then
            return "【 女剑士 】"
        elseif cla == "alchemist" then
            return "【 女方士 】"
        elseif cla == "shaman" then
            return "【 女巫医 】"
        elseif cla == "beggar" then
            return "【女叫化子】"
        else
            if ob:query("pks") > 150 then
                return "【女杀人魔】"
            elseif shen >= 500000 then
                return "【旷世女侠】"
            elseif shen >= 50000 then
                return "【 女大侠 】"
            elseif shen >= 5000 then
                return "【 女  侠 】"
            elseif shen >= 500 then
                return "【 女少侠 】"
            elseif shen <= -500000 then
                return "【 女魔王 】"
            elseif shen <= -50000 then
                return "【 女大魔 】"
            elseif shen <= -5000 then
                return "【 女  魔 】"
            elseif shen <= -500 then
                return "【 女少魔 】"
            elseif ob:query("thief") > ob:query_skill("stealing") + 10 then
                return "【女 惯 窃】"
            else
                return "【 平  民 】"
            end
        end
    else
        if ob:query("weiwang") > 100 then
            return "【盖世英豪】"
        end

        if score > 500 then
            if score < 2000 then
                return "【 小  贩 】"
            elseif score < 15000 then
                return "【 商  人 】"
            elseif score < 50000 then
                return "【大 商 人】"
            else
                return "【红顶商人】"
            end
        end

        if cla == "bonze" then
            if budd >= 150 then
                return "【 长  老 】"
            elseif budd >= 120 then
                return "【 圣  僧 】"
            elseif budd >= 90 then
                return "【 罗  汉 】"
            elseif budd >= 60 then
                return "【 尊  者 】"
            elseif budd >= 40 then
                return "【 禅  师 】"
            elseif budd >= 30 then
                return "【 比  丘 】"
            else
                return "【 僧  人 】"
            end
        elseif cla == "taoist" then
            return "【 道  士 】"
        elseif cla == "bandit" then
            return "【 盗  贼 】"
        elseif cla == "scholar" then
            return "【 书  生 】"
        elseif cla == "officer" then
            return "【 官  差 】"
        elseif cla == "fighter" then
            return "【 武  者 】"
        elseif cla == "swordsman" then
            return "【 剑  士 】"
        elseif cla == "alchemist" then
            return "【 方  士 】"
        elseif cla == "shaman" then
            return "【 巫  医 】"
        elseif cla == "beggar" then
            return "【 叫化子 】"
        else
            if ob:query("pks") > 150 then
                return "【 杀人魔 】"
            elseif shen >= 500000 then
                return "【旷世大侠】"
            elseif shen >= 50000 then
                return "【 大  侠 】"
            elseif shen >= 5000 then
                return "【 侠  客 】"
            elseif shen >= 500 then
                return "【 少  侠 】"
            elseif shen <= -500000 then
                return "【 魔  王 】"
            elseif shen <= -50000 then
                return "【 大  魔 】"
            elseif shen <= -5000 then
                return "【 魔  头 】"
            elseif shen <= -500 then
                return "【 少  魔 】"
            elseif ob:query("thief") > ob:query_skill("stealing") + 10 then
                return "【 惯  窃 】"
            else
                return "【 平  民 】"
            end
        end
    end
end

-- 尊称：rank_info/respect 覆写优先，否则按性别 / class / age
function M.respect(ob)
    local str = ob:querystr("rank_info/respect")
    if str ~= "" then
        return str
    end

    local age = ob:query("age")
    local cla = ob:querystr("class")

    if ob:querystr("gender") == "女性" then
        if cla == "bonze" then
            if age < 18 then
                return "小师太"
            else
                return "师太"
            end
        elseif cla == "taoist" then
            if age < 18 then
                return "小仙姑"
            else
                return "仙姑"
            end
        else
            if age < 18 then
                return "小姑娘"
            elseif age < 50 then
                return "姑娘"
            else
                return "婆婆"
            end
        end
    else
        if cla == "bonze" then
            if age < 18 then
                return "小师父"
            else
                return "大师"
            end
        elseif cla == "taoist" then
            if age < 18 then
                return "道兄"
            else
                return "道长"
            end
        elseif cla == "fighter" or cla == "swordsman" then
            if age < 18 then
                return "小老弟"
            elseif age < 50 then
                return "壮士"
            else
                return "老前辈"
            end
        else
            if age < 20 then
                return "小兄弟"
            elseif age < 50 then
                return "壮士"
            else
                return "老爷子"
            end
        end
    end
end

-- 蔑称：rank_info/rude 覆写优先
function M.rude(ob)
    local str = ob:querystr("rank_info/rude")
    if str ~= "" then
        return str
    end

    local age = ob:query("age")
    local cla = ob:querystr("class")

    if ob:querystr("gender") == "女性" then
        if cla == "bonze" then
            return "贼尼"
        elseif cla == "taoist" then
            return "妖女"
        else
            if age < 30 then
                return "小贱人"
            else
                return "死老太婆"
            end
        end
    else
        if cla == "bonze" then
            if age < 50 then
                return "死秃驴"
            else
                return "老秃驴"
            end
        elseif cla == "taoist" then
            return "死牛鼻子"
        else
            if age < 20 then
                return "小王八蛋"
            elseif age < 50 then
                return "臭贼"
            else
                return "老匹夫"
            end
        end
    end
end

-- 谦称（自称）：rank_info/self 覆写优先
function M.self(ob)
    local str = ob:querystr("rank_info/self")
    if str ~= "" then
        return str
    end

    local age = ob:query("age")
    local cla = ob:querystr("class")

    if ob:querystr("gender") == "女性" then
        if cla == "bonze" then
            if age < 50 then
                return "贫尼"
            else
                return "老尼"
            end
        else
            if age < 30 then
                return "小女子"
            else
                return "妾身"
            end
        end
    else
        if cla == "bonze" then
            if age < 50 then
                return "贫僧"
            else
                return "老纳"
            end
        elseif cla == "taoist" then
            return "贫道"
        else
            if age < 50 then
                return "在下"
            else
                return "老头子"
            end
        end
    end
end

-- 傲慢自称：rank_info/self_rude 覆写优先
function M.self_rude(ob)
    local str = ob:querystr("rank_info/self_rude")
    if str ~= "" then
        return str
    end

    local age = ob:query("age")
    local cla = ob:querystr("class")

    if ob:querystr("gender") == "女性" then
        if cla == "bonze" then
            if age < 50 then
                return "贫尼"
            else
                return "老尼"
            end
        else
            if age < 30 then
                return "本姑娘"
            else
                return "老娘"
            end
        end
    else
        if cla == "bonze" then
            if age < 50 then
                return "大和尚我"
            else
                return "老和尚我"
            end
        elseif cla == "taoist" then
            return "本山人"
        else
            if age < 50 then
                return "大爷我"
            else
                return "老子"
            end
        end
    end
end

-- 比较 me 与 ob 的资历，返回用于称呼 ob 的兄/弟/姐/妹。
-- 对方是玩家（有 mud_age）用在线时长比，否则退回年龄。ob 为 nil 时按 me 年龄自比。
local function compare_age(me, ob)
    local a1
    local a2
    if ob then
        a2 = ob:query("mud_age")
        if a2 ~= 0 then
            a1 = me:query("mud_age")
        else
            a1 = me:query("age")
            a2 = ob:query("age")
        end
    else
        a1 = me:query("age")
        a2 = 0
    end
    return a1, a2
end

function M.close(me, ob)
    local a1, a2 = compare_age(me, ob)

    if not ob then
        if a1 < 50 then
            return "弟弟"
        else
            return "哥哥"
        end
    end

    if ob:querystr("gender") == "女性" then
        if a1 >= a2 then
            return "妹妹"
        else
            return "姐姐"
        end
    else
        if a1 >= a2 then
            return "弟弟"
        else
            return "哥哥"
        end
    end
end

-- 以 me 视角自称兄/弟/姐/妹（对 ob）
function M.self_close(me, ob)
    local a1, a2 = compare_age(me, ob)

    if me:querystr("gender") == "女性" then
        if a1 >= a2 then
            return "姐姐我"
        else
            return "小妹我"
        end
    else
        if a1 >= a2 then
            return "愚兄我"
        else
            return "小弟我"
        end
    end
end

-- 外貌描述：按性别与容貌值 per 给文案；无匹配返回空串
function M.waimao(gender, per)
    if gender == "男性" or gender == "无性" then
        local s = WAIMAO_MALE[per]
        if s then
            return s
        end
        if per <= 10 then
            return "生得眉歪眼斜，瘌头癣脚，不象人样。"
        end
        if per >= 40 then
            return "现在一派神人气度，仙风道骨，举止出尘。"
        end
    end

    if gender == "女性" then
        local s = WAIMAO_FEMALE[per]
        if s then
            return s
        end
        if per >= 40 then
            return "现在宛如玉雕冰塑，似梦似幻，已不再是凡间人物"
        end
        if per <= 10 then
            return "生得丑如无盐，状如夜叉，女人长成这样真是人生悲剧。"
        end
    end

    return ""
end

-- 钩子入口：按 phase 分派到对应称谓函数。
-- 角色类 phase 的 a 是对象、b 是对方（可 nil）；waimao 的 a 是性别字符串、b 是 per。
function M.rule(phase, a, b)
    if phase == "rank" then
        return M.rank(a)
    elseif phase == "respect" then
        return M.respect(a)
    elseif phase == "rude" then
        return M.rude(a)
    elseif phase == "self" then
        return M.self(a)
    elseif phase == "self_rude" then
        return M.self_rude(a)
    elseif phase == "close" then
        return M.close(a, b)
    elseif phase == "self_close" then
        return M.self_close(a, b)
    elseif phase == "waimao" then
        return M.waimao(a, b)
    end
    return ""
end

-- 把称谓钩子装进全局 hooks 表（供 data/hooks.lua 调用）
function M.install(hooks)
    hooks.rank_rule = M.rule
end

return M

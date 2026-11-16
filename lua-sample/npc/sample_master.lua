-- sample_master.lua  半山腰师父
-- 示例NPC：展示师父NPC的简单实现
-- 使用download()机制，师父会什么就教什么

local npc = {}

function npc.create(self)
    self:set_name("青云子", "qing yun zi")
    self:set("title", "隐世高手")

    self:set("gender", "男性")
    self:set("age", 60)
    self:set("long", "一位仙风道骨的老者，隐居在半山腰，武功深不可测。")
    self:set("attitude", "peaceful")

    self:set("str", 40)
    self:set("con", 40)
    self:set("dex", 40)
    self:set("int", 40)

    self:set("max_hp", 50000)
    self:set("hp", 50000)
    self:set("max_mp", 20000)
    self:set("mp", 20000)
    self:set("mp_factor", 100)

    self:set("combat_exp", 10000000)

    -- 随机聊天概率(%)，同时阻止心跳关闭
    self:set("chat_chance", 3)

    -- 创建门派
    self:create_family("青云门", 1, "开山祖师")

    -- 师父自身的武功（会什么就教什么）
    self:set_skill("force", 500)
    self:set_skill("force_adv", 500)
    self:set_skill("unarmed", 500)
    self:set_skill("unarmed_adv", 500)
    -- 基本/高级掌法：空手组合（双槽 prepare 一次两招）的教学面——
    -- 徒弟学齐 unarmed_adv + hand_adv、enable 到两个空手槽后 /prepare 组合
    self:set_skill("hand", 500)
    self:set_skill("hand_adv", 500)
    self:set_skill("parry", 500)
    self:set_skill("parry_adv", 500)
    self:set_skill("dodge", 500)
    self:set_skill("dodge_adv", 500)
    self:set_skill("sword", 500)
    self:set_skill("sword_adv", 500)
    self:set_skill("throwing", 500)
    self:set_skill("throw_adv", 500)
    self:set_skill("literate", 400)
    self:set_skill("poison", 400)

    self:skill_map("set", "force", "force_adv")
    self:skill_map("set", "unarmed", "unarmed_adv")
    self:skill_map("set", "parry", "parry_adv")
    self:skill_map("set", "dodge", "dodge_adv")
    self:skill_map("set", "sword", "sword_adv")
    self:skill_map("set", "throwing", "throw_adv")

    self:skill_prepare("set", "unarmed", "unarmed_adv")

    self:carry_object("sample_sword_adv"):wield()
    self:carry_object("sample_chess")
end

function npc.attempt_apprentice(self, me)
    -- 检查叛师次数
    if me:query("betrayer") >= 2 then
        self:say("你叛师次数太多了，我不能收你。", me)
        self:send_menu(me)
        return 0
    end

    -- 检查是否已经有门派
    if me:querystr("family/family_name") ~= "" and me:querystr("family/family_name") ~= "青云门" then
        self:say("你已经拜入其他门派，我不能收你。", me)
        self:send_menu(me)
        return 0
    end

    -- 检查是否已经是本门弟子
    if me:querystr("family/family_name") == "青云门" then
        self:say("你已经是我的弟子了，好好修炼吧。", me)
        self:send_menu(me)
        return 0
    end

    -- 收徒
    self:say("好，从今天起你就是我青云门的弟子了。", me)
    self:command("recruit " .. me:object_id())
    self:download(me)
    -- 入门礼：师父讲法口传心授，弟子历练大进（每号一次）
    if me:query("reward/exp") < 1 then
        me:add("combat_exp", 50000)
        me:set("reward/exp", 1)
        self:say("为师今日传你本门口诀，好好参悟。", me)
    end
    self:send_menu(me)
    return 1
end

function npc.chat_msg(self)
    local r = math.random(5)
    if r == 1 then
        return "武功之道，在于勤修苦练，不可急功近利。"
    elseif r == 2 then
        return "内功心法，乃武学根基，不可忽视。"
    elseif r == 3 then
        return "剑法讲究身剑合一，心到剑到。"
    elseif r == 4 then
        return "轻功身法，可助你在江湖中来去自如。"
    else
        return "拳脚功夫，看似简单，实则蕴含无穷变化。"
    end
end

-- 问话入口：只注册「悔过」「验看」两条问话，其余交回 base_do_talk，
-- 保持对陌生问话的原有回绝。
--   悔过：判师记录（betrayer）的清除入口——机制全在引擎 remove_betrayer
--     （清记录，按判师次数把全部武功每重降 20 级），本脚本只管菜单与前后文。
--   验看：弟子的门规与实学核验，取值机制见 npc.handle_inspect。
function npc.do_talk(self, me, ask)
    if ask ~= "悔过" and ask ~= "验看" then
        return self:base_do_talk(me, ask)
    end

    if ask == "验看" then
        self:add_menu_item("请师父验看根骨", "$0inspect $1", me)
    else
        self:add_menu_item("我愿意承受代价，洗去判师之名", "$0repent $1", me)
    end
    self:send_menu(me)
    return 1
end

-- 验看：弟子的门规与实学核验（say 第二参收件人，只发问者本人）。
--   门规四问各设合格线 20：臂力/根骨（attr("str")/attr("con")）、悟性
--   （attr("int")）、身法（attr("dex")），任一不足即回绝，四问全过才继续报数。
--   实学两数是引擎实读：历练 = attr("combat_exp")；可用经验 =
--   query_valid_exp——未分配的经验（经验总额减去等级与技能已耗），
--   学技能与升级都从这里扣，弟子升过几级便扣掉几级的累计门槛。
function npc.handle_inspect(self, me)
    if me:attr("str") < 20 or me:attr("con") < 20 then
        self:say("你根骨不济，入不得我青云门。", me)
        return
    end
    if me:attr("int") < 20 then
        self:say("你悟性不足，先去多读几年书。", me)
        return
    end
    if me:attr("dex") < 20 then
        self:say("你身法太僵，先去练练腿脚。", me)
        return
    end

    self:say("门规尽过——根骨、悟性、身法都够格。", me)
    self:say(
        string.format("你眼下历练%d点，可用经验%d点。", me:attr("combat_exp"), me:query_valid_exp()),
        me
    )
end

function npc.handle_action(self, comm, me, arg)
    if comm == "repent" then
        if me:query("betrayer") == 0 then
            self:say("你并无判师记录，无过可悔。", me)
            return 1
        end

        self:say("悔过的代价先说清：判师几次，全部武功便每重降 20 级。", me)
        me:remove_betrayer(me)
        self:say("判师记录已消。", me)
        return 1
    end

    if comm == "inspect" then
        npc.handle_inspect(self, me)
        return 1
    end

    return self:base_handle_action(comm, me, arg)
end

return npc

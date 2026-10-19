-- sample_master.lua  半山腰师父
-- 示例NPC：展示师父NPC的简单实现
-- 使用DownLoad()机制，师父会什么就教什么

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
    self:set_skill("parry", 500)
    self:set_skill("parry_adv", 500)
    self:set_skill("dodge", 500)
    self:set_skill("dodge_adv", 500)
    self:set_skill("sword", 500)
    self:set_skill("sword_adv", 500)
    self:set_skill("throwing", 500)
    self:set_skill("throw_adv", 500)
    self:set_skill("literate", 400)

    self:map_skill("force", "force_adv")
    self:map_skill("unarmed", "unarmed_adv")
    self:map_skill("parry", "parry_adv")
    self:map_skill("dodge", "dodge_adv")
    self:map_skill("sword", "sword_adv")
    self:map_skill("throwing", "throw_adv")

    self:prepare_skill("unarmed", "unarmed_adv")

    self:carry_object("sample_sword_adv"):wield()
    self:carry_object("sample_chess")

    self:set("no_kill", 1)
end

function npc.attempt_apprentice(self, me)
    -- 检查叛师次数
    if me:query("betrayer") >= 2 then
        self:say("你叛师次数太多了，我不能收你。", me)
        self:SendMenu(me)
        return 0
    end

    -- 检查是否已经有门派
    if me:querystr("family/family_name") ~= "" and me:querystr("family/family_name") ~= "青云门" then
        self:say("你已经拜入其他门派，我不能收你。", me)
        self:SendMenu(me)
        return 0
    end

    -- 检查是否已经是本门弟子
    if me:querystr("family/family_name") == "青云门" then
        self:say("你已经是我的弟子了，好好修炼吧。", me)
        self:SendMenu(me)
        return 0
    end

    -- 收徒
    self:say("好，从今天起你就是我青云门的弟子了。", me)
    self:command("recruit " .. me:object_id())
    self:DownLoad(me)
    self:SendMenu(me)
    return 1
end

function npc.chat_msg(self)
    local r = random(5)
    if r == 0 then
        return "武功之道，在于勤修苦练，不可急功近利。"
    elseif r == 1 then
        return "内功心法，乃武学根基，不可忽视。"
    elseif r == 2 then
        return "剑法讲究身剑合一，心到剑到。"
    elseif r == 3 then
        return "轻功身法，可助你在江湖中来去自如。"
    else
        return "拳脚功夫，看似简单，实则蕴含无穷变化。"
    end
end

return npc

-- sample_bandit_low.lua  小喽啰
-- 示例NPC：展示战斗NPC的简单实现

local npc = {}

function npc.create(self)
    local names = { "山贼甲", "山贼乙", "山贼丙", "山贼丁" }
    local idx = random(4) + 1
    self:set_name(names[idx], "shan zei")

    self:set("gender", "男性")
    self:set("age", 25)
    self:set("long", "一个面相凶恶的山贼，手持钢刀。")
    self:set("attitude", "aggressive")
    self:set("shen_type", -1)

    self:set("level", 8)
    self:set("combat_exp", 50000)

    self:set("max_hp", 3000)
    self:set("hp", 3000)
    self:set("max_mp", 500)
    self:set("mp", 500)
    self:set("mp_factor", 30)

    self:set("str", 20)
    self:set("con", 20)
    self:set("dex", 20)
    self:set("int", 10)

    -- 战斗技能
    self:set_skill("sword", 30 + random(20))
    self:set_skill("force", 20 + random(10))
    self:set_skill("dodge", 25 + random(15))
    self:set_skill("parry", 25 + random(15))

    self:map_skill("sword", "sword")
    self:map_skill("force", "force")
    self:map_skill("dodge", "dodge")
    self:map_skill("parry", "parry")

    self:carry_object("sample_sword_basic"):wield()

    self:set("apply/armor", 20)
    self:set("apply/damage", 10)

    -- 战斗中自动出招概率
    self:set("chat_chance_combat", 20)
end

function npc.chat_msg_combat(self)
    -- 战斗中有概率自动出招
    if random(3) == 0 then
        self:perform_action("force recover", 0) -- 自疗
    elseif random(3) == 0 then
        self:perform_action("sword qi", 1) -- 剑气（如果会的话）
    end
end

return npc

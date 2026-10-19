-- sample_vendor.lua  山脚商人
-- 示例NPC：展示商人NPC的简单实现

local npc = {}
npc.type = "vendor"

function npc.create(self)
    self:set_name("杂货老板", "huo ji")

    self:set("gender", "男性")
    self:set("age", 40)
    self:set("long", "一个精明的商人，在山脚开着一间杂货铺。")
    self:set("attitude", "friendly")
    self:set("no_kill", 1)

    self:set("combat_exp", 5000)

    -- 上架商品
    self:carry_good("sample_sword_basic", 10)
    self:carry_good("sample_sword_adv", 3)
    self:carry_good("sample_book_force", 5)
    self:carry_good("sample_book_sword", 5)
    self:carry_good("sample_heal", 200)
    self:carry_good("sample_hp", 200)
    self:carry_good("sample_poison", 5)
    self:carry_good("sample_teleport", 2)
    self:carry_good("sample_teapot", 5)
    self:carry_good("sample_chess", 20)

    -- 自身装备
    self:carry_object("sample_sword_basic"):wear()
end

return npc

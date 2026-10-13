-- sample_sword_basic.lua  铁剑
-- 示例物品：展示基础武器的简单实现

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("铁剑")

        self:set("wield_position", WIELD_RIGHT_HAND)
        self:set("skill_type", "sword")
        self:set("base_weight", 5000)

        self:set("unit", "柄")
        self:set("long", "这是一柄普通的铁剑，虽然不算锋利，但足以防身。")
        self:set("value", 200)
        self:set("material", "iron")
        self:set("wield_msg", "$N「唰」的一声抽出一柄$n握在手中。")
        self:set("unwield_msg", "$N将手中的$n插回剑鞘。")

        self:set("apply/damage", 10)
    end,
}

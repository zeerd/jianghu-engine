-- sample_sword_adv.lua  青锋剑
-- 示例物品：展示高级武器的简单实现

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("青锋剑")

        self:set("wield_position", WIELD_RIGHT_HAND)
        self:set("skill_type", "sword")
        self:set("base_weight", 4000)

        self:set("unit", "柄")
        self:set("long", "这是一柄削铁如泥的青锋剑，剑身泛着淡淡的青光。")
        self:set("value", 2000)
        self:set("material", "steel")
        self:set("wield_msg", "$N「锵」的一声拔出一柄$n，寒光四射。")
        self:set("unwield_msg", "$N将手中的$n小心翼翼地收回剑鞘。")

        self:set("apply/damage", 30)
        self:set("apply/dex", 2)
    end,
}

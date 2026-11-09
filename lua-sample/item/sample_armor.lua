-- sample_armor.lua  皮胸甲
-- 示例物品：胸甲（wield_position=WIELD_ARMOR 落铠甲槽）——防具类装备靠
-- apply/* 给穿戴者加临时属性，卸下即撤。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("皮胸甲", "pi xiong jia")

        self:set("wield_position", WIELD_ARMOR)

        self:set("base_weight", 3000)
        self:set("unit", "件")
        self:set("long", "一件结实的皮胸甲，护住要害，行动仍轻便。")
        self:set("value", 400)
        self:set("material", "leather")
        self:set("wear_msg", "$N穿上一件$n。")
        self:set("remove_msg", "$N脱下身上的$n。")

        self:set("apply/con", 1)
    end,
}

-- sample_boots.lua  布鞋
-- 示例物品：鞋子（wield_position=WIELD_BOOT 落鞋子槽）——穿戴类装备靠
-- apply/* 给穿戴者加临时属性，卸下即撤。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("布鞋", "bu xie")

        self:set("wield_position", WIELD_BOOT)

        self:set("base_weight", 1000)
        self:set("unit", "双")
        self:set("long", "一双轻便的布鞋，走起路来悄无声息。")
        self:set("value", 150)
        self:set("material", "cloth")
        self:set("wear_msg", "$N穿上一双$n。")
        self:set("remove_msg", "$N脱下脚上的$n。")

        self:set("apply/dex", 1)
    end,
}

-- sample_hat.lua  青巾
-- 示例物品：帽子（wield_position=WIELD_HEAD 落头部槽）——穿戴类装备靠
-- apply/* 给穿戴者加临时属性，卸下即撤。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("青巾", "qing jin")

        self:set("wield_position", WIELD_HEAD)

        self:set("base_weight", 300)
        self:set("unit", "顶")
        self:set("long", "一方青色儒巾，戴上顿觉心思清明。")
        self:set("value", 200)
        self:set("material", "cloth")
        self:set("wear_msg", "$N束起一顶$n。")
        self:set("remove_msg", "$N解下头上的$n。")

        self:set("apply/int", 2)
    end,
}

-- sample_necklace.lua  玉项链
-- 示例物品：项链（wield_position=WIELD_NECK 落颈部槽）——首饰类装备靠
-- apply/* 给穿戴者加临时属性，卸下即撤。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("玉项链", "yu xiang lian")

        self:set("wield_position", WIELD_NECK)

        self:set("base_weight", 200)
        self:set("unit", "条")
        self:set("long", "一条温润的玉项链，贴身佩戴，气质自生。")
        self:set("value", 500)
        self:set("material", "jade")
        self:set("wear_msg", "$N戴上一条$n。")
        self:set("remove_msg", "$N取下颈间的$n。")

        self:set("apply/per", 1)
    end,
}

-- sample_chess.lua  棋子

return {
    type = "equip",
    can_combine = true,
    can_combine_in_box = false,

    create = function(self, flag)
        self:set_name("棋子")

        self:set("wield_position", WIELD_RIGHT_HAND)
        self:set("skill_type", "throwing")

        self:set("unit", "盒")
        self:set("value", 2000)
        self:set("base_unit", "枚")
        self:set("base_weight", 300)
        self:set("base_value", 1)
        self:set("material", "iron")
        self:set("long", "一把棋子，有黑有白。")
        self:set("wield_msg", "$N迅速地从怀中夹出一枚棋子！")
        self:set("unwield_msg", "$N将手里剩下的棋子放回怀中。")
        self:set_amount(50)
        -- 暗器的实打伤害走投掷技能线（持械者 apply/技能），此值只在被握持
        -- 时作为武器加成——与铁剑同档，不因一叠棋子越级
        self:set("apply/damage", 10)
    end,
}

-- sample_short_sword.lua  短剑
-- 示例物品：左手兵器的另一型——势沉不可连发（无 can_combo），换更高的
-- 攻击加成（apply/damage 高于匕首）。与 sample_dagger.lua 成对演示左手
-- 双持的取舍：匕首快而轻（连击），短剑沉而稳（单发更重）。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("短剑", "duan jian")

        self:set("wield_position", WIELD_LEFT_HAND)
        self:set("skill_type", "sword")
        -- 不设 can_combo：势沉，快手不追击

        self:set("base_weight", 4000)
        self:set("unit", "柄")
        self:set("long", "一柄沉手的短剑，剑脊厚实，劈砍势大力沉。")
        self:set("value", 500)
        self:set("material", "iron")
        self:set("wield_msg", "$N反手扣住剑柄，一柄短剑压入左手。")
        self:set("unwield_msg", "$N左手一振，短剑插回鞘中。")

        -- 势沉定位：高于匕首 5、低于主手铁剑 10——副手不越主手
        self:set("apply/damage", 8)
    end,
}

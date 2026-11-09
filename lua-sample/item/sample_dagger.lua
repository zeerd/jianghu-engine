-- sample_dagger.lua  匕首
-- 示例物品：左手兵器（wield_position=WIELD_LEFT_HAND 落左槽，命令无槽参数）
-- + 可连发标记 can_combo——与 data/hooks.lua 的「左手连击」配套：轻便短兵，
-- 常规出手后快手能再补一记；右手照常持剑，双持分工（短剑见
-- sample_short_sword.lua：势沉不可连发，换更高的攻击加成）。

return {
    type = "equip",

    create = function(self, nFlag)
        self:set_name("匕首", "bi shou")

        self:set("wield_position", WIELD_LEFT_HAND)
        self:set("skill_type", "sword")
        -- 轻便可连发：hooks.fight 左手连击的判据
        self:set("can_combo", 1)

        self:set("base_weight", 500)
        self:set("unit", "柄")
        self:set("long", "一柄轻便的短匕首，贴身藏袖，出手极快。")
        self:set("value", 300)
        self:set("material", "iron")
        self:set("wield_msg", "$N袖口一翻，一柄匕首滑入左手。")
        self:set("unwield_msg", "$N左手一松，匕首收回袖中。")

        -- 轻快定位：单发加成全场最低（低于铁剑 10），输出靠 can_combo 连击补
        self:set("apply/damage", 5)
    end,
}

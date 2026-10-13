-- sample_teapot.lua  茶壶
-- 示例物品：展示 liquid 类型物品的用法
-- 使用 liquid_base 模块提供饮用/装填逻辑

local liquid_base = require("item.liquid_base")

return {
    create = function(self, nFlag)
        self:set_name("茶壶", "cha hu")
        self:set_weight(500)
        self:set("unit", "把")
        self:set("long", "一把粗陶茶壶，壶身还带着炉火的余温。")
        self:set("value", 50)

        -- 液体属性
        self:set("max_liquid", 10)
        self:set("liquid/type", "tea")
        self:set("liquid/name", "清茶")
        self:set("liquid/remaining", 0)
        self:set("liquid/drunk_supply", 3)
    end,

    -- 挂载 liquid_base 提供的通用逻辑
    do_look = liquid_base.do_look,
    do_use = liquid_base.do_use,
    Can_Use = liquid_base.Can_Use,
    handle_action = liquid_base.handle_action,
}

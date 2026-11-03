-- sample_baozi.lua  包子
-- 示例物品：展示 food_base（lua-default/item/food_base.lua）通用食物逻辑的复用。
-- 脚本只声明物品数据，「能不能吃、回多少血气、吃完销毁」全部交给公共模块——
-- 对照 sample_heal.lua（自带 do_use）：能复用通用逻辑就不重写一遍。

local food_base = require("item.food_base")

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("包子", "bao zi")
        self:set("unit", "个")
        self:set("long", "一个热气腾腾的包子。")
        self:set("value", 200)

        -- food_base 的食物字段：food_remaining 是还能吃几口（本品一口即尽）；
        -- food_supply 是总供给——单口回血取 floor(food_supply / 3)，吃完走
        -- 「吃得干干净净」的文案分支也由它（>0）决定
        self:set("food_remaining", 1)
        self:set("food_supply", 15)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = food_base.Can_Use,

    -- use 命令入口：恢复 hp/mp，food_remaining 吃尽后销毁物品
    do_use = food_base.do_use,
}

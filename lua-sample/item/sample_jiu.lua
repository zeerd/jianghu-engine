-- sample_jiu.lua  烈酒
-- 示例物品：展示 liquid 类型物品的「致醉」分支。
--
-- 与 sample_teapot（酒壶）的分工：酒壶是**空容器**，装什么由 fill 说了算——
--   在茶棚装清茶就走 water/tea 分饮分支，不产生任何状态；本坛出厂即满，
--   type=alcohol，饮用直接落进 liquid_base.do_use 的醉酒分支：喝完挂上
--   condition/drunk 状态（条件脚本见 condition/drunk.lua），此后醉意逐拍推进，
--   直到昏过去、再由状态自己醒过来。
-- 即饮即醉是这条链的全部：没有 fill、没有次数限制，一次 /use 就走完全程。

local liquid_base = require("item.liquid_base")

return {
    create = function(self, nFlag)
        self:set_name("烈酒", "lie jiu")

        self:set("unit", "坛")
        self:set("long", "一坛未启封的烈酒，凑近了就能闻到冲鼻的酒气。")
        self:set("value", 80)

        -- 液体属性：出厂即满的酒精饮料，饮一口扣一份（do_use 递减）
        self:set("max_liquid", 5)
        self:set("liquid/type", "alcohol")
        self:set("liquid/name", "烧刀子")
        self:set("liquid/remaining", 5)
        -- 每口贡献的醉意拍数：condition/drunk 逐拍推进，拍数越长醉得越久
        self:set("liquid/drunk_supply", 2)
    end,

    -- 挂载 liquid_base 提供的通用逻辑（观察 / 饮用 / 可用 / 填充分发）
    do_look = liquid_base.do_look,
    do_use = liquid_base.do_use,
    Can_Use = liquid_base.Can_Use,
    handle_action = liquid_base.handle_action,
}

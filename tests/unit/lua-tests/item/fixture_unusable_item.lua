-- fixture_unusable_item.lua  测试专用：实现了 do_use 但显式禁用的脚本
--
-- 显式 Can_Use 优先于 do_use 判定。
return {
    type = "item",
    name = "显式禁用的道具",

    create = function(self, nFlag)
        self:set("li_created", 1)
    end,

    do_use = function(self, me, target)
        return 1
    end,

    Can_Use = function(self)
        return 0
    end,
}

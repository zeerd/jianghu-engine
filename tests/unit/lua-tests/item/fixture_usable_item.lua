-- fixture_usable_item.lua  测试专用：实现 do_use 但未声明 Can_Use 的脚本
--
-- 可用性由「是否实现使用行为」决定：缺 Can_Use 时按 do_use 判定。
return {
    type = "item",
    name = "只有使用行为的道具",

    create = function(self, nFlag)
        self:set("li_created", 1)
    end,

    do_use = function(self, me, target)
        return 1
    end,
}

-- fixture_resident_room.lua  测试专用：安装居民区通用逻辑的住宅区
--
-- 供 test_resident_lua.cpp 验证 room.resident_base 的购房登记、管理费
-- 结算、住户名录与卖房迁出经由模块直调（house_price / count_days /
-- update_register）与房间钩子（do_look / handle_action）生效。
local resident_base = require("room.resident_base")

return resident_base.install({
    type = "room",
    name = "测试住宅区",

    create = function(self)
        self:set_name("测试住宅区")
        self:set("resident", "测试城")
        self:set("long", "沿街是一排青砖小院。")
    end,
})

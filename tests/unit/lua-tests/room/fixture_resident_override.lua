-- fixture_resident_override.lua  测试专用：住宅区脚本自带钩子
--
-- 供 test_resident_lua.cpp 验证 resident_base.install 只补齐缺失钩子：
-- 脚本自带 do_look 时不被缺省实现覆盖。
local resident_base = require("room.resident_base")

return resident_base.install({
    type = "room",
    name = "覆盖住宅区",

    create = function(self)
        self:set_name("覆盖住宅区")
        self:set("resident", "覆盖城")
    end,

    do_look = function(self, me)
        self:say("脚本自带的视野。", me)
        return 1
    end,
})

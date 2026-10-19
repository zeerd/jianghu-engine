-- fixture_resident_auto.lua  测试专用：type = "resident" 自动装配的住宅区
--
-- 脚本不调用 install：供 test_resident_lua.cpp 验证引擎在房间脚本加载期
-- 按 type = "resident" 自动混入 room/resident_base（房间钩子与
-- resident_area / can_save 字段随装配生效）。
return {
    type = "resident",
    name = "自动装配住宅区",

    create = function(self)
        self:set_name("自动装配住宅区")
        self:set("resident", "装配城")
        self:set("long", "新砌的粉墙围着一排小院。")
    end,
}

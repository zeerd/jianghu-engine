-- fixture_resident_no_install.lua  测试专用：声明 type = "resident" 却不自装配
--
-- 供 test_resident_lua.cpp 验证引擎不再按 type 自动混入 room/resident_base：
-- 装配已改为脚本自装配（require + install），本脚本刻意不调 install，
-- 房间钩子与 resident_area / can_save 字段都不应出现。
return {
    type = "resident",
    name = "未装配住宅区",

    create = function(self)
        self:set_name("未装配住宅区")
        self:set("resident", "装配城")
        self:set("long", "新砌的粉墙围着一排小院。")
    end,
}

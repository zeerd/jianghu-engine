-- t_error_fb.lua  测试夹具：每个回调都抛错的副本总控
-- 供 tests/test_lua_error_paths.cpp 验证 CLuaRoomT<CFB> 的报错契约：
-- CFB 系基类带 create(int)/desc，且没有 valid_leave，三条标签分派路径
-- 各自的报错分支都由本夹具触发。
return {
    type = "fb",
    name = "报错副本",

    create = function(self, flag)
        self:set_name("报错副本")
        self:set("long", "一座专门用来演示脚本报错的副本。")
        error("create boom")
    end,

    valid_leave = function(self, me, dir)
        error("valid_leave boom")
    end,

    desc = function(self, me)
        error("desc boom")
    end,
}

-- t_error_room.lua  测试夹具：每个回调都抛错的房间
-- 供 tests/test_lua_error_paths.cpp 验证 CLuaRoomT<CRoom> 的报错契约。
-- create 先置好地名再抛错，房间仍可被加载器产出，仅报错分支被触发。
return {
    type = "room",
    name = "报错房间",

    create = function(self)
        self:set_name("报错房间")
        self:set("long", "一间专门用来演示脚本报错的房间。")
        error("create boom")
    end,

    setup = function(self)
        error("setup boom")
    end,

    reset = function(self)
        error("reset boom")
    end,

    init = function(self, me)
        error("init boom")
    end,

    do_look = function(self, me)
        error("do_look boom")
    end,

    handle_action = function(self, comm, me, arg, tar)
        error("handle_action boom")
    end,

    valid_leave = function(self, me, dir)
        error("valid_leave boom")
    end,

    desc = function(self, me)
        error("desc boom")
    end,
}

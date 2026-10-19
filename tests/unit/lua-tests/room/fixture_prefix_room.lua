-- fixture_prefix_room.lua  测试专用：动态实例化的通用房间脚本夹具
--
-- 含 prefix="fxroom" 与 can_save=true（供 test_dynamic_room.cpp 验证
-- 前缀解析与存档恢复时 CAN_SAVE 由脚本字段参与）。
-- handle_action/valid_leave/do_look/desc 钩子供 LuaRoom.inl 深路用例回读。
-- 文件位于 lua-tests，测试环境可直接走游戏数据加载。
return {
    type = "room",
    name = "测试前缀房间",
    prefix = "fxroom",
    can_save = true,

    create = function(self)
        self:set("fx_created", 1)
    end,

    handle_action = function(self, comm, me, arg, tar)
        return 7
    end,

    valid_leave = function(self, me, dir)
        return 0
    end,

    do_look = function(self, me)
        return 1
    end,

    desc = function(self, me)
        self:set("desc_ran", 1)
    end,

    reset = function(self)
        self:set("reset_ran", 1)
    end,
}
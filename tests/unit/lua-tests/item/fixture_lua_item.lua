-- fixture_lua_item.lua  测试专用：带全套钩子的 CLuaItem 脚本
--
-- 字段名与 LuaItem.inl 的 lua_getfield 一一对应（大小写敏感）。
return {
    type = "item",
    name = "测试道具",
    can_save = false,

    create = function(self, nFlag)
        self:set("li_created", 1)
    end,

    do_use = function(self, me, target)
        return 1
    end,

    do_look = function(self, me)
        return 1
    end,

    Can_Use = function(self)
        return 1
    end,

    Can_Equip = function(self, me)
        return 1
    end,

    query_autoload = function(self)
        return 1
    end,

    handle_action = function(self, comm, me, arg)
        return 1
    end,

    handle_action4 = function(self, comm, me, arg, tar)
        return 1
    end,

    do_action = function(self, act, me)
        return 1
    end,

    can_combine = true,

    can_combine_in_box = function(self)
        return true
    end,
}
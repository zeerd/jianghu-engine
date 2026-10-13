-- sample_xp.lua  经验果

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("经验果", "jing yan dan")
        self:set("unit", "粒")
        self:set("long", "这是一粒散发着淡淡药香的经验果。")
        self:set("value", 500)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃，小心别噎着了。")
        end

        me:add("combat_exp", 100000)
        me:FlushMyInfo()
        message_vision("$N吃下一粒经验果。", me)
        me:start_busy(2)
        destruct(self)

        return 1
    end,
}

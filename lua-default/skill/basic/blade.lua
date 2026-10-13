-- blade.lua  基本刀法

return {
    create = function(self)
        self:set_name("基本刀法")
    end,

    query_action = function(self, me)
        return { action = "$N挥舞$w砍在$n的脑袋上。" }
    end,
}

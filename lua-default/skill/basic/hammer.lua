-- hammer.lua  基本锤法

return {
    create = function(self)
        self:set_name("基本锤法")
    end,

    query_action = function(self, me)
        return { action = "$N举起$w砸在$n的脑袋上。" }
    end,
}

-- staff.lua  基本杖法

return {
    create = function(self)
        self:set_name("基本杖法")
    end,

    query_action = function(self, me)
        return { action = "$N挥舞$w打在$n的脑袋上。" }
    end,
}

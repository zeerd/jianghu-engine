-- club.lua  基本棍法

return {
    create = function(self)
        self:set_name("基本棍法")
    end,

    query_action = function(self, me)
        return { action = "$N挥舞$w往$n扫过去。" }
    end,
}

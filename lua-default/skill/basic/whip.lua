-- whip.lua  基本鞭法

return {
    create = function(self)
        self:set_name("基本鞭法")
    end,

    query_action = function(self, me)
        return { action = "$N挥舞$w打在$n的身上。", dodge = -20, parry = 30 }
    end,
}

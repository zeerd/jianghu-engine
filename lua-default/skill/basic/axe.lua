-- axe.lua  基本斧法

return {
    create = function(self)
        self:set_name("基本斧法")
    end,

    query_action = function(self, me)
        return { action = "$N挥舞$w向$n劈去。", dodge = 30, parry = 30 }
    end,
}

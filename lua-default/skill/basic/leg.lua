-- leg.lua  基本腿法

return {
    create = function(self)
        self:set_name("基本腿法")
    end,

    query_action = function(self, me)
        return { action = "$N向$n一脚接一脚的踢了过去。", dodge = -10, damage = 1 }
    end,
}

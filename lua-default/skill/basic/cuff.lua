-- cuff.lua  基本拳法

return {
    create = function(self)
        self:set_name("基本拳法")
    end,

    query_action = function(self, me)
        return { action = "$N一拳打在$n的身上。" }
    end,
}

-- hoof.lua  蹬

return {
    -- create(self)
    create = function(self)
        self:set_name("蹬")
    end,

    -- query_action(self, me)
    query_action = function(self, me)
        local level = me:query("level")
        local damage = 30 + random(71) + level * 2
        return {
            action = "$N用后腿往$n的$l用力一蹬",
            damage = damage,
        }
    end,

    -- valid_combine(self, combo)
    valid_combine = function(self, combo)
        return 1
    end,

    -- valid_enable(self, usage)
    valid_enable = function(self, usage)
        return 1
    end,

    -- valid_force(self, arg)
    valid_force = function(self, arg)
        return 1
    end,
}

-- knock.lua  撞

return {
    -- create(self)
    create = function(self)
        self:set_name("撞")
    end,

    -- query_action(self, me)
    query_action = function(self, me)
        local level = me:query("level")
        local damage = 70 + random(31) + level * 2
        return {
            action = "$N用头往$n的$l用力一撞",
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

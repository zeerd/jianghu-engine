return {
    -- create(self)
    create = function(self)
        self:set_name("咬")
    end,

    -- query_action(self, me)
    query_action = function(self, me)
        local level = me:query("level")
        local damage = 20 + random(81) + level * 2
        return {
            action = "$N扑上来张嘴往$n的$l狠狠地一咬",
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

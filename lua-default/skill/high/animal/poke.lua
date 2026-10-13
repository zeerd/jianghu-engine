-- poke.lua  啄

local skill_utils = require("skill.skill_utils")

return {
    -- create(self)
    create = function(self)
        self:set_name("啄")
    end,

    -- query_action(self, me)
    query_action = function(self, me)
        local level = me:query("level")
        local damage = 30 + random(71) + level * 2
        return {
            action = "$N用嘴往$n的$l一啄",
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

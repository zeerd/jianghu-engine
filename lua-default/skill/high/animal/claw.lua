local skill_utils = require("skill.skill_utils")

return {
    create = function(self)
        self:set_name("抓")
    end,

    query_action = function(self, me)
        local level = me:query("level")
        local damage = 1 + random(100) + level * 2
        return {
            action = "$N用爪子往$n的$l一抓",
            damage = damage,
        }
    end,

    valid_combine = function(self, combo)
        return 1
    end,

    valid_enable = function(self, usage)
        return 1
    end,

    valid_force = function(self, arg)
        return 1
    end,
}

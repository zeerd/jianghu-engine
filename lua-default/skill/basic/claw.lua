-- claw.lua  基本爪法

return {
    create = function(self)
        self:set_name("基本爪法")
    end,

    query_action = function(self, me)
        return { action = "$N伸出五指爪在$n的脸上。" }
    end,
}

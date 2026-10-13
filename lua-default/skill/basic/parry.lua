-- parry.lua  基本招架

return {
    create = function(self)
        self:set_name("基本招架")
    end,

    query_parry_msg = function(self, weapon)
        return "但是被$n挡住了。"
    end,
}

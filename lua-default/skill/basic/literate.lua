-- literate.lua  读书写字

return {
    create = function(self)
        self:set_name("读书写字")
    end,

    valid_learn = function(self, me)
        return 1
    end,

    type = function(self)
        return "knowledge"
    end,
}

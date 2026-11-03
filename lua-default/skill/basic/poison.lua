-- poison.lua  基本毒技

return {
    -- create(self)
    create = function(self)
        self:set_name("基本毒技")
    end,

    -- type(self)：技能类型
    type = function(self)
        return "knowledge"
    end,
}

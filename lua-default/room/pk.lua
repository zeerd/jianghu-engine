-- pk.lua  PK场

return {
    type = "room",
    name = "pk",

    create = function(self)
        self:set_name("PK场")
        self:set("PK", 1)
        self:set("valid_startroom", 1)
        self:set("sleep_room", 1)
        self:set("wiz_room", 1)
    end,
}

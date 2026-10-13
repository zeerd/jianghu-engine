-- room_void.lua  时空隧道

return {
    type = "room",
    name = "room_void",

    create = function(self)
        self:set_name("时空隧道")
    end,

    handle_action = function(self, comm, me, arg, tar)
        if comm == "get" or comm == "drop" or comm == "give" or comm == "use" then
            return 1
        end
        return 0
    end,
}

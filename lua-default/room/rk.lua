-- wiz_rk.lua  游戏大厅

return {
    type = "room",
    name = "rk",

    create = function(self)
        self:set_name("游戏大厅")
        self:set("wiz_room", 1)
    end,

    handle_action = function(self, comm, me, arg, target)
        if
            comm ~= "card"
            and comm ~= "createcard"
            and comm ~= "addcard"
            and comm ~= "removecard"
            and comm ~= "showcards"
            and comm ~= "quit"
            and comm ~= "mudage"
            and comm ~= "password"
            and comm ~= "validate"
        then
            tell_object(
                me,
                "你尝试了一下，但是发现这里似乎没有信号。（请输入/card命令获取进一步指示）"
            )
            return 1
        end
        return 0
    end,
}

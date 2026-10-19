-- jy.lua  监狱

return {
    type = "room",
    name = "jy",

    create = function(self)
        self:set_name("监狱")
        self:set("time", 18000)
        self:set("wiz_room", 1)
    end,

    handle_action = function(self, comm, me, arg, target)
        -- grant < 50 的玩家在监狱里，只能执行特定命令
        if me:query("grant") < 50 then
            if (comm == "say" and me:query_temp("jy/no_say") == 0) or me:query_temp("jy/free") ~= 0 then
                return 0
            end
            tell_object(me, "别忘了，你在蹲监狱！还是乖点吧。")
            return 1
        end
        return 0
    end,
}

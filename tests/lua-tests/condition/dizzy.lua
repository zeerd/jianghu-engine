-- dizzy（测试夹具：眩晕）

return {
    update_condition = function(self, me, duration)
        if duration < 1 then
            return 0
        end
        duration = duration - 1
        return CND_CONTINUE, duration
    end,
}

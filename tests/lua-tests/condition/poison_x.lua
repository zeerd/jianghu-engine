-- poison_x（测试夹具：通用中毒变体）

return {
    update_condition = function(self, me, duration)
        if duration < 1 then
            return 0
        end
        duration = duration - 1
        return CND_CONTINUE, duration
    end,
}

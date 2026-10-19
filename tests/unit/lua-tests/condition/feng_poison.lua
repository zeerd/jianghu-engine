-- feng_poison：投掷淬毒的中毒条件（夹具），与 poison 同构

return {
    update_condition = function(self, me, duration)
        if duration < 1 then
            return 0
        end
        duration = duration - 1
        return CND_CONTINUE, duration
    end,
}

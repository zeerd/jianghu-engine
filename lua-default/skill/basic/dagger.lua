-- dagger.lua  基本短兵

return {
    create = function(self)
        self:set_name("基本短兵")
    end,

    query_action = function(self, me)
        -- 穿喉、仙女引针、白猿献果、叶底偷桃，双蝶舞花
        return { action = "$N挥舞$w点在$n的身上。" }
    end,
}

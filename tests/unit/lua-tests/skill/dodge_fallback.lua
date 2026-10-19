-- dodge_fallback.lua  测试专用：无 query_dodge_msg、带 query_action 函数
-- 用于 resolve_dodge 的 211-215 fallback 分支。
return {
    create = function(self)
        self:set_name("闪避回退样本")
    end,

    query_action = function(self, me)
        return {
            action = "$n轻巧地闪开了。",
            dodge = 30,
        }
    end,
}
-- parry_adv.lua  高级格挡
-- 示例技能：展示高级格挡，含卸力绝招

return {
    create = function(self)
        self:set_name("高级格挡")
    end,

    query_parry_msg = function(self, weapon)
        local level = 0
        if self then
            -- 尝试获取使用者的技能等级
        end
        return "但是被$n巧妙地格开了。"
    end,

    valid_enable = function(self, usage)
        return (usage == "parry") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("parry", 1)
        if basic < 10 then
            return notify_fail("你的基本格挡火候不够，无法修炼高级格挡。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 15 or me:query("mp") < 5 then
            return notify_fail("你的体力或内力不够，无法练习高级格挡。")
        end
        me:receive_damage("hp", 8)
        me:add("mp", -3)
        return 1
    end,
}

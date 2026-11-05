-- parry_adv.lua  高级格挡
-- 示例技能：展示高级格挡，含卸力绝招

return {
    create = function(self)
        self:set_name("高级格挡")
        -- 卸力：战斗中展开的卸力架势，施放即叠防御增益，收势时机不是拍数
        -- 而是「战斗结束」（非敌意的自身增益，不挂 PK 标记）
        self:add_magic("xieli", "卸力", 40, 100, CAST_TO_ME, CAST_IN_COMBAT, "do_xieli", 0)
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

    -- do_xieli(self, me, target)：卸力——给自己叠一层防御增益（apply/defense
    -- 是格挡、闪避共用的防御判定加成），到战斗结束才收势。
    -- 施放时机只在战斗中：状态的解除挂在这场战斗的结束上，脱战起势没有
    -- 解除时机，增益会一直挂着不散。
    do_xieli = function(self, me, target)
        if not me:is_fighting() then
            return notify_fail("卸力只在与人交手时才能施展。")
        end
        if me:query_temp("xieli") ~= 0 then
            return notify_fail("你已经运起卸力了。")
        end
        if me:query_skill("parry_adv", 1) < 10 then
            return notify_fail("你的高级格挡火候不够，无法施展卸力。")
        end
        if me:query("mp") < 40 then
            return notify_fail("你的内力不足，无法施展卸力。")
        end

        local bonus = me:query_skill("parry_adv", 1) // 10
        me:add("mp", -40)
        me:set_temp("xieli", 1)
        me:add_temp("apply/defense", bonus)
        message_vision("$N沉肩坠肘，将周身来力尽数卸开。", me)
        -- 统一写法（docs/call_out.md）：延迟函数写在发起 call_out 的脚本里，
        -- 在效果作用的对象上注册；delay=0 的 call_out_combat 心跳不走表，
        -- 只等这场战斗结束（角色离开战斗房）时被引擎触发，
        -- 加成值经 param1 传下去，收势时原样回扣
        me:call_out_combat("xieli_end", 0, bonus)
        return 1
    end,

    -- 卸力收势（延迟函数：执行时 self = 挂状态的角色）
    -- param1: 施放时的加成值，直接回扣相同数量，不假设拥有整个 apply/defense
    xieli_end = function(self, param1, param2)
        self:add_temp("apply/defense", -param1)
        self:delete_temp("xieli")
        message_vision("$N吐气开声，卸力架势散去。", self)
    end,
}

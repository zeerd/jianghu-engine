-- unarmed_adv.lua  高级拳脚
-- 示例技能：展示高级拳脚，含连环击绝招

return {
    create = function(self)
        self:set_name("高级拳脚")
        -- 连环击：连续攻击目标
        self:add_magic("combo", "连环击", 30, 100, CAST_TO_OTHER, CAST_IN_COMBAT, "do_combo")
    end,

    query_action = function(self, me)
        local level = me:query_skill("unarmed_adv", 1)
        local actions = {
            { action = "$N使出一招「连续快攻」，拳如雨点般打向$n。", lvl = 0 },
            { action = "$N身形一变，左右开弓，打得$n节节败退。", lvl = 20 },
            { action = "$N双拳齐出，一招「双龙出海」击向$n的要害。", lvl = 50 },
        }

        for i = #actions, 1, -1 do
            if actions[i].lvl < level then
                return { action = actions[random(i) + 1].action }
            end
        end
        return { action = actions[1].action }
    end,

    valid_enable = function(self, usage)
        return (usage == "unarmed" or usage == "parry") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("unarmed", 1)
        if basic < 10 then
            return notify_fail("你的基本拳脚火候不够，无法修炼高级拳脚。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 25 or me:query("mp") < 5 then
            return notify_fail("你的体力或内力不够，无法练习高级拳脚。")
        end
        me:receive_damage("hp", 15)
        me:add("mp", -5)
        return 1
    end,

    -- 连环击
    do_combo = function(self, me, target)
        if not target or not target:is_character() then
            return notify_fail("只能对战斗中的对手使用。")
        end
        if me:query_skill("unarmed_adv", 1) < 50 then
            return notify_fail("你的高级拳脚修为不够，无法施展连环击。")
        end
        if me:query("mp") < 30 then
            return notify_fail("你的内力不足，无法施展连环击。")
        end
        me:add("mp", -30)

        local hits = 3 + math.floor(me:query_skill("unarmed_adv", 1) / 50)
        for i = 1, hits do
            if not target or target:query("hp") <= 0 then
                break
            end
            local damage = me:query_skill("unarmed_adv", 1) + random(20)
            target:receive_damage("hp", damage)
            message_vision("$N第" .. i .. "击打在$n身上！", me, target)
        end
        me:start_busy(2)
        return 1
    end,
}

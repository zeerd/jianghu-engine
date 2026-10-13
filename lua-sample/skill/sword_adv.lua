-- sword_adv.lua  高级剑法
-- 示例技能：展示高级剑法，含剑气三连绝招

return {
    create = function(self)
        self:set_name("高级剑法")
        -- 剑气：三连攻击
        self:add_magic("qi", "剑气三连", 100, 100, CAST_TO_OTHER, CAST_IN_COMBAT, "do_qi")
    end,

    query_action = function(self, me)
        local level = me:query_skill("sword_adv", 1)
        local actions = {
            {
                action = "$N使一招「白虹贯日」，手中$w化作一道寒光，直取$n。",
                dodge = -40,
                damage = 80,
                lvl = 0,
            },
            {
                action = "$N剑随身转，一招「无边落木」罩向$n的全身。",
                dodge = -30,
                damage = 120,
                lvl = 20,
            },
            {
                action = "$N手中$w龙吟一声，祭出「平沙落雁」往$n刺出数剑。",
                dodge = -20,
                damage = 160,
                lvl = 50,
            },
            {
                action = "$N手中$w化成一道光弧，一招「长虹经天」直指$n。",
                dodge = -40,
                damage = 200,
                lvl = 80,
            },
        }

        for i = #actions, 1, -1 do
            if actions[i].lvl < level then
                local idx = random(i)
                local a = actions[idx + 1]
                return {
                    action = a.action,
                    dodge = a.dodge,
                    damage = a.damage,
                }
            end
        end
        return { action = actions[1].action, dodge = actions[1].dodge, damage = actions[1].damage }
    end,

    valid_enable = function(self, usage)
        return (usage == "sword" or usage == "parry") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("sword", 1)
        if basic < 10 then
            return notify_fail("你的基本剑法火候不够，无法修炼高级剑法。")
        end
        local weapon = me:query_weapon()
        if not weapon or weapon:querystr("skill_type") ~= "sword" then
            return notify_fail("你必须手持剑类武器才能学习高级剑法。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 30 or me:query("mp") < 10 then
            return notify_fail("你的体力或内力不够，无法练习高级剑法。")
        end
        me:receive_damage("hp", 15)
        me:add("mp", -5)
        return 1
    end,

    -- 剑气三连
    do_qi = function(self, me, target)
        if not target or not target:is_character() then
            return notify_fail("只能对战斗中的对手使用。")
        end
        if me:query_skill("sword_adv", 1) < 60 then
            return notify_fail("你的高级剑法修为不够，无法施展剑气三连。")
        end
        if me:query("mp") < 100 then
            return notify_fail("你的内力不足，无法施展剑气三连。")
        end

        message_vision("$N运足内力，将真气灌注剑身，连刺三道凌厉的剑气！", me)

        -- 第一剑
        combat_attack(me, target, TYPE_QUICK)
        -- 第二剑
        combat_attack(me, target, TYPE_QUICK)
        -- 第三剑
        combat_attack(me, target, TYPE_QUICK)

        me:add("mp", -100)
        me:start_busy(random(2))
        return 1
    end,
}

-- throw_adv.lua  高级投掷
-- 示例技能：展示高级投掷，含飞星连珠绝招

return {
    create = function(self)
        self:set_name("高级投掷")
        -- 飞星连珠：连续投掷攻击
        self:add_magic("feixing", "飞星连珠", 80, 100, CAST_TO_OTHER, CAST_IN_COMBAT, "do_feixing")
    end,

    query_action = function(self, me)
        local level = me:query_skill("throw_adv", 1)
        local actions = {
            {
                action = "$N手腕一抖，一枚$w破空射出，直取$n咽喉。",
                dodge = -30,
                damage = 70,
                lvl = 0,
            },
            {
                action = "$N双手齐扬，三枚$w呈品字形飞向$n要害。",
                dodge = -25,
                damage = 110,
                lvl = 20,
            },
            {
                action = "$N身形微侧，$w划出一道诡异弧线，袭向$n背心「灵台穴」。",
                dodge = -35,
                damage = 150,
                lvl = 50,
            },
            {
                action = "$N大喝一声，满天$w如流星雨般笼罩$n全身退路。",
                dodge = -40,
                damage = 190,
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
        return (usage == "throwing" or usage == "parry") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("throwing", 1)
        if basic < 10 then
            return notify_fail("你的基本投掷火候不够，无法修炼高级投掷。")
        end
        local weapon = me:query_weapon()
        if not weapon or weapon:querystr("skill_type") ~= "throwing" then
            return notify_fail("你必须手持暗器类武器才能学习高级投掷。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 30 or me:query("mp") < 10 then
            return notify_fail("你的体力或内力不够，无法练习高级投掷。")
        end
        me:receive_damage("hp", 15)
        me:add("mp", -5)
        return 1
    end,

    -- 飞星连珠
    do_feixing = function(self, me, target)
        if not target or not target:is_character() then
            return notify_fail("只能对战斗中的对手使用。")
        end
        if me:query_skill("throw_adv", 1) < 60 then
            return notify_fail("你的高级投掷修为不够，无法施展飞星连珠。")
        end
        if me:query("mp") < 80 then
            return notify_fail("你的内力不足，无法施展飞星连珠。")
        end

        message_vision("$N凝神聚气，指尖暗劲涌动，连珠般打出数枚$w！", me)

        -- 第一枚
        combat_attack(me, target, TYPE_QUICK)
        -- 第二枚
        combat_attack(me, target, TYPE_QUICK)
        -- 第三枚
        combat_attack(me, target, TYPE_QUICK)

        me:add("mp", -80)
        me:start_busy(random(2))
        return 1
    end,
}

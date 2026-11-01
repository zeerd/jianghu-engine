-- sword_adv.lua  高级剑法
-- 示例技能：展示高级剑法，含剑气三连绝招——剑招与掌招交替的组合绝招
-- （武器半边 + 空手半边，两半各有独立前置，缺一即整招不发）

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

    -- do_qi(self, me, target)：剑气三连——剑招与掌招交替的组合绝招
    --   武器半边走 combat_attack（按手中武器的 skill_type 取招），
    --   空手半边走 combat_attack_unarmed（攻击位即 hand 槽：招式与力道都取
    --   自激发到 hand 的技能，没激发就只剩基础手法的巴掌）。两半各有独立前置，
    --   缺一即整招不发。
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

        -- 武器半边：手里必须是剑，且基本剑法要有火候——否则起手与收尾
        --   两记落不到 sword_adv 的出招表上
        local weapon = me:query_weapon()
        if not weapon or weapon:querystr("skill_type") ~= "sword" then
            return notify_fail("你手里没有剑，无法施展剑气三连。")
        end
        if me:query_skill("sword", 1) < 10 then
            return notify_fail("你的基本剑法火候不够，无法施展剑气三连。")
        end

        -- 空手半边：掌法要有火候底子，且必须既激发（enable 到 hand 槽——
        --   combat_attack_unarmed 以 hand 为攻击位取招与力道）又准备
        --   （prepare，组合绝招要求空手招已上槽），缺一即整招不发
        if me:query_skill("hand_adv", 1) < 10 then
            return notify_fail("你的掌法火候不够，无法施展剑气三连。")
        end
        if me:query_skill_mapped("hand") ~= "hand_adv" then
            return notify_fail("你没有激发高级掌法，不能在剑招中夹杂使用剑气三连。")
        end
        if me:query_skill_prepared("hand") ~= "hand_adv" then
            return notify_fail("你没有准备好高级掌法，不能在剑招中夹杂使用剑气三连。")
        end

        local palm_level = me:query_skill("hand_adv", 1)

        me:add("mp", -100)
        -- 绝招运劲：三击期间出手抬升（按掌法火候计），收招回扣
        me:add_temp("apply/attack", palm_level // 4)

        -- 第一击：武器招
        message_vision("$N运足内力，将真气灌注剑身，第一剑疾刺而出！", me, target)
        combat_attack(me, target, TYPE_QUICK)

        -- 第二击：空手招（str 临时加成只包这一击，随即回扣）
        message_vision("$N弃剑换掌，右掌蓄劲拍向$n的胸口！", me, target)
        me:add_temp("apply/str", palm_level // 4)
        combat_attack_unarmed(me, target, "hand", TYPE_QUICK)
        me:add_temp("apply/str", -(palm_level // 4))

        -- 第三击：回到武器招
        message_vision("$N弃掌换剑，长剑回环又是一道剑气！", me, target)
        combat_attack(me, target, TYPE_QUICK)

        me:add_temp("apply/attack", -(palm_level // 4))
        -- 收招硬直：绝招的收势代价，心跳拍计时（1 拍 ≈ 1 秒）
        me:start_busy(1 + random(2))
        return 1
    end,
}

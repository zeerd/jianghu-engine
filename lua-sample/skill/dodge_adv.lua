-- dodge_adv.lua  高级轻功
-- 示例技能：展示高级轻功，含凌空虚渡绝招

return {
    create = function(self)
        self:set_name("高级轻功")
        -- 凌空虚渡：短时间内大幅提升闪避
        self:add_magic("float", "凌空虚渡", 40, 100, CAST_TO_ME, CAST_IN_COMBAT, "do_float")
    end,

    query_dodge_msg = function(self, me)
        local msgs = {
            "但是$N身形飘忽，轻松避开了。",
            "但是$N足尖一点，飘然后退。",
            "但是$N凌空一翻，躲过了攻击。",
            "但是$N身形如鬼魅般闪过。",
        }
        return msgs[random(4) + 1]
    end,

    valid_enable = function(self, usage)
        return (usage == "dodge") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("dodge", 1)
        if basic < 20 then
            return notify_fail("你的基本轻功火候不够，无法修炼高级轻功。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 15 or me:query("mp") < 5 then
            return notify_fail("你的体力或内力不够，无法练习高级轻功。")
        end
        me:receive_damage("hp", 8)
        me:add("mp", -3)
        return 1
    end,

    -- 凌空虚渡
    do_float = function(self, me, target)
        if me:query("mp") < 40 then
            return notify_fail("你的内力不足，无法施展凌空虚渡。")
        end
        me:add("mp", -40)
        message_vision("$N身形一纵，如仙人般凌空而起，轻功大进！", me)
        local dodge_bonus = 100
        me:add_temp("apply/dodge", dodge_bonus)
        -- 统一写法（docs/call_out.md）：回调写在发起 call_out 的脚本里，
        -- 在效果作用的对象上注册——对象是玩家还是脚本 NPC 写法相同
        -- 通过 param1 传递加成值，实现增量管理（不假设拥有整个 apply/dodge）
        me:call_out("do_float_end", 10, dodge_bonus)
        me:start_busy(1)
        return 1
    end,

    -- 凌空虚渡效果结束（延迟函数：执行时 self = 受效果的对象）
    -- param1: 当初加成的数值，直接回扣相同数量
    do_float_end = function(self, param1, param2)
        self:add_temp("apply/dodge", -param1)
    end,
}

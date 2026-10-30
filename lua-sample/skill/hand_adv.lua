-- hand_adv.lua  高级掌法
-- 示例技能：展示第二门空手系高级武功，与高级拳脚（unarmed_adv）构成
-- 空手组合的教学面：两技能 enable 到不同空手槽（hand / unarmed）后
-- /prepare 组合，空手回合各出一招（引擎按 prepare 槽位轮流取动作）。

return {
    create = function(self)
        self:set_name("高级掌法")
    end,

    query_action = function(self, me)
        local level = me:query_skill("hand_adv", 1)
        local actions = {
            { action = "$N单掌推出，一招「力劈华山」直取$n面门。", lvl = 0 },
            { action = "$N掌风呼啸，左右两掌连环拍向$n的胸口。", lvl = 20 },
            { action = "$N运起全身内力，一记「金刚掌」势大力沉地按向$n。", lvl = 50 },
        }

        for i = #actions, 1, -1 do
            if actions[i].lvl < level then
                return { action = actions[random(i) + 1].action }
            end
        end
        return { action = actions[1].action }
    end,

    valid_enable = function(self, usage)
        return (usage == "hand" or usage == "parry") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("hand", 1)
        if basic < 10 then
            return notify_fail("你的基本掌法火候不够，无法修炼高级掌法。")
        end
        return 1
    end,

    -- 组合判定：可与unarmed_adv组合。
    -- ⚠ 引擎（CmdStandardMaster can_prepare_combine）只查「后加入组合的那门
    -- 技能」的 valid_combine——所以想让两个方向的 /prepare 顺序都成立，
    -- 双方技能都必须声明本字段（unarmed_adv 侧同样声明）。
    valid_combine = function(self, combo)
         return (combo == "unarmed_adv") and 1 or 0
    end,

    practice_skill = function(self, me)
        if me:query("hp") < 25 or me:query("mp") < 5 then
            return notify_fail("你的体力或内力不够，无法练习高级掌法。")
        end
        me:receive_damage("hp", 15)
        me:add("mp", -5)
        return 1
    end,
}

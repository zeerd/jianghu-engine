-- sword.lua  基本剑术

return {
    create = function(self)
        self:set_name("基本剑术")
    end,

    query_action = function(self, me)
        local actions = {
            { action = "$N挥舞着$w，神采飞扬的指向$n。", dodge = 15, parry = -15 },
            { action = "$N手中$w幻化成一缕白光，直向$n的咽喉刺去。", dodge = 15, parry = -15 },
            { action = "$N步影随形，轻轻飘飘的将$w送到$n胸前。", dodge = 15, parry = -15 },
            {
                action = "$N喝道：“小子你不想活了。”说着手中$w已经指到$n面前。",
                dodge = 20,
                parry = -10,
            },
            {
                action = "$N不紧不慢，一把$w挥舞得天衣无缝，把$n看得目瞪口呆。",
                dodge = -30,
                parry = -30,
            },
            { action = "$N微微一笑，手中$w不知不觉间已送到$n的胸前。", dodge = -10, parry = -10 },
            { action = "$N身形一变，眨眼间转到$n面前，$w直戳$n的胸口。", dodge = -10, parry = -10 },
            { action = "$N高高举起手中$w，奋力的向$n的头顶劈去！", dodge = 0, parry = 20 },
        }

        local level = random(8) + 1
        return actions[level]
    end,

    valid_learn = function(self, me)
        local who = me:environment():present(me:querystr("family/master_id"))
        if who and who:query("add_skill/count") ~= 0 then
            return notify_fail("你的师傅暂时没空指点你武功。。")
        end

        local who1 = me:environment():present(me:querystr("couple/id"))
        if who1 and who1:query("add_skill/count") ~= 0 then
            return notify_fail("你暂时无法指点你伴侣的武功。。")
        end

        return 1
    end,
}

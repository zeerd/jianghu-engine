-- force_adv.lua  高级内功
-- 示例技能：展示高级内功，含自疗术和狮子吼绝招

return {
    create = function(self)
        self:set_name("高级内功")
        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName)
        -- 自疗术：对自身施放，战斗中可用（非敌意，不挂 PK 标记）
        self:add_magic("heal", "自疗术", 50, 100, CAST_TO_ME, CAST_IN_BOTH, "do_heal", 0)
        -- 狮子吼：波及全场的攻击性震慑，属敌意行为
        self:add_magic("roar", "狮子吼", 200, 100, CAST_TO_NONE, CAST_IN_COMBAT, "do_roar")
    end,

    valid_enable = function(self, usage)
        return (usage == "force") and 1 or 0
    end,

    valid_learn = function(self, me)
        local basic = me:query_skill("force", 1)
        if basic < 10 then
            return notify_fail("你的基本内功火候不够，无法修炼高级内功。")
        end
        return 1
    end,

    practice_skill = function(self, me)
        return notify_fail("高级内功只能用请教的来增加熟练度。")
    end,

    -- 自疗术
    do_heal = function(self, me, target)
        if me:query("mp") < 50 then
            return notify_fail("你的内力不足，无法施展自疗术。")
        end
        me:add("mp", -50)
        me:receive_curing("hp", 100)
        message_vision("$N盘膝而坐，运功调息，气色逐渐好转。", me)
        me:start_busy(2)
        return 1
    end,

    -- 狮子吼
    do_roar = function(self, me, target)
        if me:query("mp") < 200 then
            return notify_fail("你的内力不足，无法施展狮子吼。")
        end
        me:add("mp", -200)
        message_vision("$N深吸一口气，运足内力，发出一声震天怒吼！", me)
        -- 对周围敌人造成伤害
        local room = me:environment()
        if room then
            local list = room:Get_ItemList()
            if list then
                for obj in iter_items(list) do
                    if obj:is_character() and obj:object_id() ~= me:object_id() then
                        if me:is_fighting_target(obj) then
                            local damage = me:query_skill("force_adv", 1) * 2
                            obj:receive_damage("hp", damage)
                            message_vision("$N被狮子吼震得气血翻涌！", obj)
                        end
                    end
                end
            end
        end
        me:start_busy(3)
        return 1
    end,
}

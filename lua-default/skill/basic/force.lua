-- force.lua  基本内功

return {
    create = function(self)
        self:set_name("基本内功")

        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName
        --           [, hostile])
        -- 复元术：自身调息复元，不构成敌意行为（hostile = 0）
        self:add_magic("recover", "复元术", 20, 100, CAST_TO_ME, CAST_IN_BOTH, "do_recover", 0)
    end,

    -- hit_ob(self, me, victim, damage_bonus)
    -- 内功反震：mp_factor 越高反震越强；被反震方按伤情分四档播报
    -- 返回 (damage_bonus, 命中消息)：消息进战斗战报
    hit_ob = function(self, me, victim, damage_bonus)
        local factor = me:query("mp_factor")
        local desc = 0
        local chat = {
            "$HIY$N受到$n的内力反震，闷哼一声。",
            "$HIR$N被$n以内力反震，「嘿」地一声退了两步。",
            "$HIR$N被$n以内力一震，胸口有如受到一记重击，连退了五六步！",
            "$RED$N被$n的内力一震，眼前一黑，身子向后飞出丈许！！",
        }

        if userp(me) then
            me:add("mp", -factor)
        end

        local mp = me:query("mp")
        local mp_you = victim:query("mp")
        if mp < 0 then
            mp = 0
        end
        if mp_you < 0 then
            mp_you = 0
        end

        if mp_you > mp * 3 then
            mp_you = mp * 3
        end

        local damage = mp // 30 + factor - mp_you // 100

        if damage < 0 then
            if not me:query_weapon() and random(victim:query_skill("force") + 1) > me:query_skill("force") // 2 then
                damage = -damage
                me:receive_damage("hp", damage * 2, victim)
                me:receive_wound("hp", damage, victim)

                if damage < 10 then
                    desc = 0
                elseif damage < 20 then
                    desc = 1
                elseif damage < 40 then
                    desc = 2
                else
                    desc = 3
                end
                return damage_bonus, chat[desc + 1]
            end

            damage_bonus = damage_bonus + damage
            if damage_bonus < 0 then
                damage_bonus = 0
            end
            return damage_bonus
        end

        damage = damage - victim:query_temp("apply/armor_vs_force")

        if damage_bonus + damage < 0 then
            damage_bonus = 0
        elseif random(me:query_skill("force")) < damage then
            damage_bonus = damage_bonus + damage
        end

        return damage_bonus
    end,

    -- do_recover(self, me, target)
    do_recover = function(self, me, target)
        if me:query("mp") < 20 then
            return notify_fail("你的内力不够。")
        end

        local q = me:query("eff_hp") - me:query("hp")
        if q < 10 then
            return notify_fail("你现在气力充沛。")
        end
        if me:query_skill("force") == 0 then
            return notify_fail("你不会内功。")
        end

        local n = 100 * q // me:query_skill("force")
        if n < 20 then
            n = 20
        end
        if me:query("mp") < n then
            q = q * me:query("mp") // n
            n = me:query("mp")
        end

        me:add("mp", -n)
        me:receive_healing("hp", q)

        message_vision("$N深深吸了几口气，脸色看起来好多了。", me)

        if me:is_fighting() then
            me:start_busy(1)
        end

        return 1
    end,
}

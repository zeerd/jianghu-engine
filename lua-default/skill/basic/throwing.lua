-- throwing.lua  基本投掷

return {
    base = "throwing",

    create = function(self)
        self:set_name("基本投掷")

        -- add_magic(name, displayName, mp, ap, target, validCast, luaFuncName)
        self:add_magic("sneak", "偷袭", 30, 100, CAST_TO_OTHER, CAST_IN_BOTH, "do_sneak")
    end,

    query_action = function(self, me)
        return { action = "$N扬起$w向$n投去。" }
    end,

    do_sneak = function(self, me, ob)
        local target = nil
        if ob and ob:is_character() then
            target = ob
        end

        if (me:environment()):query("no_fight") ~= 0 then
            return notify_fail("这里禁止偷袭。")
        end

        if not target or target:object_id() == me:object_id() then
            return notify_fail("你想偷袭谁？")
        end

        if not wizardp(me) and wizardp(target) then
            return notify_fail("玩家不能偷袭巫师。")
        end

        if userp(target) then
            return notify_fail("不能偷袭玩家。")
        end

        local weapon = get_first_throwing_weapon(me)
        if not weapon then
            return notify_fail("没有暗器，怎么偷袭。")
        end

        if me:is_fighting() and target:is_fighting() and me:query_team() == target:query_team() then
            return notify_fail("不能偷袭本方队员！")
        end

        if me:query("mp") < 100 then
            return notify_fail("你的法力不足！")
        end

        local poi = me:query_skill("poison", 1)
        local weapon_name = weapon:name(1)
        local target_name = target:name()

        if poi > 0 then
            tell_object(
                me,
                string.format(
                    "你不动声色地掏出一枚淬了毒的%s，准备偷袭%s ...",
                    weapon_name,
                    target_name
                )
            )
        else
            tell_object(
                me,
                string.format("你不动声色地掏出一枚%s，准备偷袭%s ...", weapon_name, target_name)
            )
        end

        local sp, dp

        if me:is_fighting() then
            sp = me:query_skill("throwing")
            sp = random(sp)
            dp = target:query_skill("dodge")
            dp = dp // 2
        else
            sp = me:query_skill("throwing")
            sp = random(sp)
            dp = target:query_skill("dodge")
            dp = random(dp // 2)
        end

        if sp > dp then
            if me:is_fighting() then
                target:receive_damage("hp", sp + dp, me)
                me:start_busy(2)
                me:add("mp", -100)
                message_vision(
                    string.format("$p只觉一阵刺痛，受到了%d点暗器伤害！\n", sp + dp),
                    me,
                    target
                )
                if poi > 0 then
                    target:apply_condition("feng_poison", sp - dp)
                end
            else
                target:kill_ob(me)
                me:kill_ob(target)
                me:add("mp", -30)
                message_vision("$p只觉一阵刺痛，中暗器了！\n", me, target)
            end
        else
            tell_object(
                me,
                string.format("%s运气太好了，刚好一个侧身躲过了你的偷袭。", target_name)
            )
            me:add("mp", -30)
            me:start_busy(1)
        end

        throw_weapon(me, target, weapon, 0)

        return 1
    end,
}

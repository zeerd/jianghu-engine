-- sample_poison.lua  断肠散
-- 示例物品：展示毒药类物品的简单实现

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("断肠散")

        self:set("unit", "包")
        self:set("long", "这是一包剧毒的断肠散，通体乌黑，散发着刺鼻的气味。")
        self:set("value", 0)
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃。")
        end

        message_vision("$N毫不犹豫地将断肠散吞了下去。", me)

        local damage = me:query("max_hp") * 20 // 100 -- 扣除20%最大HP
        if damage < 10 then
            damage = 10
        end
        me:receive_damage("hp", damage)
        message_vision("$N脸色一变，显然毒性发作了！", me)

        -- 也有概率直接解毒（随机效果）
        if random(3) == 0 then
            message_vision("$N运功逼出毒气，反而感觉内力精进了。", me)
            me:add("max_mp", 10)
            me:add("mp", 10)
        end

        me:start_busy(3)
        destruct(self)
        return 1
    end,
}

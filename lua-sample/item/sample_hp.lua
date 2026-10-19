-- sample_hp.lua  回春丹
-- 示例物品：展示恢复当前生命值(hp)的简单实现

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("回春丹", "hui chun dan")
        self:set("unit", "粒")
        self:set("long", "这是一粒散发着淡淡药香的回春丹，据说可以恢复体力。")
        self:set("value", 500)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃，小心别噎着了。")
        end

        if me:query("hp") >= me:query("eff_hp") then
            return notify_fail("你现在不需要用回春丹。")
        end

        local eff_hp = me:query("eff_hp")
        local hp = me:query("hp")
        local heal_amount = math.min(100, eff_hp - hp)
        me:receive_healing("hp", heal_amount)
        me:FlushMyInfo()
        message_vision("$N吃下一粒回春丹，精神立刻好了许多。", me)
        me:start_busy(2)
        destruct(self)

        return 1
    end,
}

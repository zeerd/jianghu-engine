-- sample_heal.lua  金创药
-- 示例物品：展示回复类药品的简单实现

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("金创药", "jin chuang yao")
        self:set("unit", "包")
        self:set("long", "这是一包武林人士必备的金创药，可治疗外伤。")
        self:set("value", 300)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃，小心别噎着了。")
        end

        if me:query("eff_hp") >= me:query("max_hp") then
            return notify_fail("你现在不需要用金创药。")
        end

        me:receive_curing("hp", 50)
        me:FlushMyInfo()
        message_vision("$N吃下一包金创药，气色看起来好多了。", me)
        me:start_busy(2)
        destruct(self)

        return 1
    end,
}

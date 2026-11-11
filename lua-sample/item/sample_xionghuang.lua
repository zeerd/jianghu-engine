-- sample_xionghuang.lua  雄黄
-- 示例物品：展示「定向摘除单个状态」——apply_condition 传 duration=0 即让
-- 引擎摘掉那一项 condition（连施毒者归属键一并清）。
-- 与 sample_antidote.lua 成对：雄黄专克蛇毒（正解蛇胆 sample_shedan.lua
-- 之毒），要解所有毒得用解毒丸。

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("雄黄", "xiong huang")
        self:set("unit", "包")
        self:set("long", "一包雄黄，端午佩用的辟毒之物，专克蛇虫之毒。")
        self:set("value", 100)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃，小心别噎着了。")
        end

        if me:query_condition("snake_poison") == 0 then
            return notify_fail("你身上并没有蛇毒，雄黄留给该用的人吧。")
        end

        message("vision", "$N捏碎一撮雄黄和水服下，压住了体内的蛇毒。", me)
        -- 只摘蛇毒一项：duration=0 是解除语义，第 4 参带施加者把归属键一并清
        me:apply_condition("snake_poison", 0, 0, me)

        me:start_busy(1)
        destruct(self)
        return 1
    end,
}

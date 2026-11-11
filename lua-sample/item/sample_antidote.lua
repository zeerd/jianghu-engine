-- sample_antidote.lua  解毒丸
-- 示例物品：展示「整表解除状态」——调用 clear_condition 把身上的 condition
-- 表整个清空，毒、醉一并散去，故曰可解所有毒。
-- 与 sample_xionghuang.lua 成对：解毒丸管全身、金贵；雄黄专解蛇毒、
-- 便宜对症（蛇胆 sample_shedan.lua 之毒，两样解药的 do_use 注释互相关联）。

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("解毒丸", "jie du wan")
        self:set("unit", "粒")
        self:set("long", "一粒蜡封的解毒丸，据称可解天下百毒。")
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

        -- dbase("map", "condition") 为空 ⟺ 身上没有任何状态（引擎在最后一项
        -- 解除时会把 condition 表整个删掉）
        if not me:dbase("map", "condition") then
            return notify_fail("你身上并没有毒，用不着解毒丸。")
        end

        message("vision", "$N仰头吞下一丸解毒药，只觉周身毒气散尽。", me)
        -- 解的是全身状态表：蛇毒、酒醉一并散去
        me:clear_condition()

        me:start_busy(1)
        destruct(self)
        return 1
    end,
}

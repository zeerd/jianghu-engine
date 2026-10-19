-- sample_poison.lua  断肠散
-- 示例物品：展示毒药类物品的简单实现
-- 伤害分两段：服下即造成双损伤（气血 + 气血上限），随后挂载「断肠散之毒」
-- 状态（条件脚本见 condition/sample_poison.lua），毒性随心跳逐拍继续侵蚀
-- 气血，直至毒性自行散去。

return {
    type = "food",

    create = function(self, nFlag)
        self:set_name("断肠散")

        self:set("unit", "包")
        self:set("long", "这是一包剧毒的断肠散，通体乌黑，散发着刺鼻的气味。")
        self:set("value", 0)
    end,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    do_use = function(self, me, target)
        if me:is_busy() then
            return notify_fail("别急，慢慢吃。")
        end

        message_vision("$N毫不犹豫地将断肠散吞了下去。", me)

        -- 即时双损伤（均为确定性数值）：
        -- hp 扣 40% 上限值 → 供回春丹 (receive_healing) 的真实生效分支；
        -- eff_hp 扣 10% 上限值 → 供金创药 (receive_curing) 的真实生效分支。
        -- 深度扣血拉开与心跳回血 (con/3 每拍) 的余量，避免时序抖动影响判定。
        -- 服下这一记是「立即伤害」，毒性真正的持续伤害由「断肠散之毒」状态
        -- 承担（见 condition/sample_poison.lua），两段合计约 45% 上限值。
        local max_hp = me:query("max_hp")

        local hp_damage = max_hp * 40 // 100
        if hp_damage < 10 then
            hp_damage = 10
        end
        local eff_damage = max_hp * 10 // 100
        if eff_damage < 10 then
            eff_damage = 10
        end

        me:receive_damage("hp", hp_damage)
        me:add("eff_hp", -eff_damage)
        message_vision("$N脸色一变，显然毒性发作了！", me)

        -- 挂载「断肠散之毒」状态：条件脚本见 condition/sample_poison.lua。
        -- duration 的单位是心跳拍数（每拍约 5~9 秒），2 拍后毒性自行散去。
        me:apply_condition("sample_poison", 2, 0)

        me:FlushMyInfo()

        me:start_busy(3)
        destruct(self)
        return 1
    end,
}

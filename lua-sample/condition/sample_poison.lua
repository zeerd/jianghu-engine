-- sample_poison.lua  断肠散之毒（状态脚本）
--
-- 示例条件脚本：演示「状态（condition）」机制。
--   1. 物品脚本用 apply_condition("sample_poison", N) 把状态挂到角色身上，
--      引擎随即调用本脚本的 init() 一次（播报中毒）；
--   2. 之后每次心跳引擎调用 update_condition()，脚本在这里施加持续伤害，
--      并把 duration 递减写回；
--   3. duration 归零时返回 0（不带 CND_CONTINUE），引擎清除该条件项，
--      毒性散尽。
--
-- 条件名即脚本名：condition/<条件名>.lua，本文件对应条件名 sample_poison。
-- 注意：duration 的单位是「心跳拍数」而不是秒——角色心跳约每 5~9 秒一拍。

return {
    -- 挂载瞬间调用一次：播报中毒
    init = function(self, me)
        tell_object(me, "断肠散的毒性渗入了你的体内。")
        return 1
    end,

    -- 每拍调用一次：duration 为剩余拍数
    update_condition = function(self, me, duration)
        if duration <= 1 then
            tell_object(me, "毒性渐渐散去，你的身体恢复了正常。")
            return 0
        end

        -- 毒性持续侵蚀气血：按气血上限的 5% 计，随角色成长同步放大
        local damage = me:query("max_hp") * 5 // 100
        if damage < 1 then
            damage = 1
        end
        me:receive_damage("hp", damage)
        tell_object(me, "毒性一阵阵发作，你的身体又虚弱了几分。")
        return CND_CONTINUE, duration - 1
    end,
}

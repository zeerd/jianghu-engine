-- snake_poison.lua  蛇毒（状态脚本）
--
-- 示例条件脚本：演示「状态（condition）」机制——**可叠加的有害条件**。
--   1. 物品脚本用 apply_condition("snake_poison", 当前值 + poison_supply)
--      把状态挂到角色身上，引擎随即调用本脚本的 init() 一次（播报中毒）；
--      多次服用会累加 duration，吞越多毒性越久。
--   2. 之后每次心跳引擎调用 update_condition()，脚本在这里施加持续伤害，
--      并把 duration 递减写回；
--   3. duration 归零时返回 0（不带 CND_CONTINUE），引擎清除该条件项，
--      毒性散尽。
--
-- 条件名即脚本名：condition/<条件名>.lua，本文件对应条件名 snake_poison
-- （物品侧的挂载方见 item/sample_shedan.lua）。
-- 注意：duration 的单位是「心跳拍数」而不是秒——角色心跳约每 5~9 秒一拍。

return {
    -- 挂载瞬间调用一次：播报中毒
    init = function(self, me)
        tell_object(me, "蛇胆的毒性渗入了你的体内。")
        return 1
    end,

    -- 每拍调用一次：duration 为剩余拍数
    update_condition = function(self, me, duration)
        if duration <= 1 then
            tell_object(me, "毒性渐渐散去，你的身体恢复了正常。")
            return 0
        end

        -- 毒性持续侵蚀：按气血上限的 5% 计，随角色成长同步放大。
        -- 走 receive_poison 而不是 receive_damage：它按施毒者归属结算
        -- （condition/who_<名> 找到下毒者才吃其毒术加成，找不到按无归属处理），
        -- 落点是 wound 语义——扣气血上限 eff_hp，并把当前气血压到其下；
        -- receive_damage 则只扣当前气血 hp。两者都让气血往下走，路径不同。
        local damage = me:query("max_hp") * 5 // 100
        if damage < 1 then
            damage = 1
        end
        me:receive_poison("hp", damage, me:name(1))
        tell_object(me, "毒性一阵阵发作，你的身体又虚弱了几分。")
        return CND_CONTINUE, duration - 1
    end,
}

-- drunk.lua  醉酒（状态脚本）
--
-- 示例条件脚本：演示「状态（condition）」驱动的昏迷与苏醒——
--   1. 饮烈酒时物品侧 apply_condition("drunk", 累积拍数) 把状态挂到角色身上，
--      引擎随即调用本脚本的 init() 一次（酒意上头）；
--   2. 之后每次心跳引擎调用 update_condition()，脚本在这里推进醉意：
--      还有余量就继续播报、把拍数递减写回；
--   3. 拍数走到 1：me:unconcious() 昏过去——引擎侧同时把气血归零、禁用操作，
--      并另起一条 30~109 拍的自动苏醒计时当兜底；
--   4. 拍数走到 0：revive(me, 0) 自己醒过来、回满气血，返回 0 摘掉本状态。
-- 苏醒归本状态自己管，不等引擎那条兜底计时——醉多久由状态说了算，
-- 兜底只负责「状态没跑完人就不在了」这类意外。
--
-- 条件名即脚本名：condition/<条件名>.lua，本文件对应条件名 drunk
-- （挂载方见 item/sample_jiu.lua 的 liquid_base 饮用链）。
-- duration 的单位是「心跳拍数」而不是秒——角色心跳约每 5~9 秒走一次 update。

return {
    -- 挂载瞬间调用一次：播报酒意上头
    init = function(self, me)
        tell_object(me, "一股酒意冲上头来。")
        return 1
    end,

    -- 每拍调用一次：duration 为剩余拍数
    update_condition = function(self, me, duration)
        if duration <= 0 then
            -- 酒力散尽：醒过来并回满气血，返回 0 让引擎摘掉本状态。
            -- 回血只改服务端 dbase，客户端的气血快照要靠 flush_my_info 才推得过去
            revive(me, 0)
            me:receive_healing("hp", me:query("max_hp"))
            me:flush_my_info()
            tell_object(me, "酒醒了，你长长吐出一口气。")
            return 0
        end

        if duration == 1 then
            -- 醉到头了：昏过去。状态留到下一拍（CND_CONTINUE），
            -- 由下一拍的 duration <= 0 分支负责苏醒
            me:unconcious()
            return CND_CONTINUE, 0
        end

        tell_object(me, "你觉得一阵酒意上冲，眼皮有些沉重了。")
        return CND_CONTINUE, duration - 1
    end,
}

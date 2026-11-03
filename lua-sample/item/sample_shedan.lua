-- sample_shedan.lua  蛇胆
-- 示例物品：展示毒药类物品的简单实现
-- 伤害分两段：服下即造成双损伤（气血 + 气血上限），随后挂载「蛇毒」状态
-- （条件脚本见 condition/snake_poison.lua），毒性随心跳逐拍继续侵蚀
-- 气血，直至毒性自行散去。
-- 来路有两条：青蛇击杀后由其 die 掉落在地（见 npc/sample_qingshe.lua）、
-- 山脚杂货铺也有售（见 npc/sample_vendor.lua）。

local poison_base = require("item.poison_base")

return {
    type = "poison",

    create = function(self, nFlag)
        self:set_name("蛇胆", "she dan")

        self:set("unit", "颗")
        self:set("long", "一颗从青蛇体内取出的蛇胆，气味腥苦，吞下立时发作。")
        self:set("value", 0)
        self:set("poison_supply", 2)
        -- 毒药面（poison_base.do_look / do_mo）要用的两个字段：
        -- poison_type 即 condition 脚本名——涂到手上/兵器上挂的就是它
        -- （服下也挂同一个状态，见 handle_action 的 use 分支）；
        -- index 是涂毒可用次数，服下整颗销毁、与它无关
        self:set("poison_type", "snake_poison")
        self:set("index", 3)
    end,

    -- 毒药的观察面直接来自 poison_base（显示剩余次数 + 涂毒菜单）
    do_look = poison_base.do_look,
    -- do_mo 收尾的 me:call_out("remove_effect") 按名在**本物品脚本表**里查
    -- 回调（注册期校验，缺失即报错），故必须一并挂出来，涂毒时效才登记得上
    remove_effect = poison_base.remove_effect,

    -- 声明可使用：客户端据此显示「使用」选项，use 命令也据此放行
    Can_Use = function(self)
        return 1
    end,

    handle_action = function(self, comm, me, arg)
        if comm == "use" then
            if me:is_busy() then
                return notify_fail("别急，慢慢吃。")
            end

            message_vision("$N毫不犹豫地将蛇胆吞了下去。", me)

            -- 即时双损伤（均为确定性数值）：
            -- hp 扣 40% 上限值 → 供回春丹 (receive_healing) 的真实生效分支；
            -- eff_hp 扣 10% 上限值 → 供金创药 (receive_curing) 的真实生效分支。
            -- 深度扣血拉开与心跳回血 (con/3 每拍) 的余量，避免时序抖动影响判定。
            -- 服下这一记是「立即伤害」，毒性真正的持续伤害由「蛇毒」状态
            -- 承担（见 condition/snake_poison.lua），两段合计约 45% 上限值。
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

            -- 挂载「蛇毒」状态：条件脚本见 condition/snake_poison.lua。
            -- duration 的单位是心跳拍数（每拍约 5~9 秒），每颗贡献 poison_supply 拍，
            -- 多颗叠加，吞越多毒性越久。
            local current = me:query_condition("snake_poison")
            local add = self:query("poison_supply") or 2 -- 每颗贡献的时长
            me:apply_condition("snake_poison", current + add, 0)

            me:FlushMyInfo()

            me:start_busy(3)
            destruct(self)
            return 1
        end
        if comm == "tumo" then
            return poison_base.do_mo(self, me)
        end
        return 0
    end,
}

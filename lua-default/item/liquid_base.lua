-- liquid_base.lua 液体物品通用逻辑
-- 将原 CLiquid (Liquid.cpp) 的逻辑 Lua 化
-- 放在 lua-default 作为基础公共模块，lua-sample/item 下的液体脚本通过 require 引用

local liquid_base = {}

-- 计算液体剩余描述
function liquid_base.extra_long(self)
    local amount = self:query("liquid/remaining")
    if amount and amount > 0 then
        local max = self:query("max_liquid")
        local name = self:querystr("liquid/name")
        if amount == max then
            return string.format("里面装满了%s。", name)
        elseif amount > max // 2 then
            return string.format("里面装了七、八分满的%s。", name)
        elseif amount >= max // 3 then
            return string.format("里面装了五、六分满的%s。", name)
        else
            return string.format("里面装了少许的%s。", name)
        end
    else
        return "里面一滴也没有了。"
    end
end

-- 查看物品
function liquid_base.do_look(self, me)
    if not me then
        return 0
    end
    self:say(self:Short() .. " " .. self:Long(), me)
    self:say(liquid_base.extra_long(self), me)
    return 0
end

-- 是否可以使用
function liquid_base.Can_Use(self)
    return 1
end

-- 使用（饮用）
function liquid_base.do_use(self, me, target)
    if not me then
        return 1
    end

    local msg = self:querystr("drink_msg")

    if not living(me) then
        return 1
    end
    if me:is_busy() then
        return notify_fail("你上一个动作还没有完成。")
    end

    if self:query("liquid/remaining") == 0 then
        local liquid_name = self:querystr("liquid/name")
        if liquid_name and #liquid_name > 0 then
            return notify_fail(string.format("%s已经被喝得一滴也不剩了。", self:name()))
        else
            return notify_fail(string.format("%s是空的。", self:name()))
        end
    end

    self:add("liquid/remaining", -1)

    if not msg or #msg == 0 then
        msg = string.format("$N拿起$n咕噜噜地喝了几口%s。", self:querystr("liquid/name"))
    end

    message_vision(msg, me, self)

    me:add("hp", 1)
    if me:is_fighting() then
        me:start_busy(2)
    end

    if self:query("liquid/remaining") == 0 then
        me:tell_object(
            string.format(
                "你已经将%s里的%s喝得一滴也不剩了。",
                self:name(),
                self:querystr("liquid/name")
            )
        )
    end

    -- This allows customization of drinking effect.
    if self:query("liquid/drink_func") then
        return 1
    end

    local liquid_type = self:querystr("liquid/type")
    if liquid_type == "alcohol" then
        message_vision("$N的脸色一阵红一阵白，好象是喝醉了！", me)
        me:apply_condition("drunk", me:query_condition("drunk") + (self:query("liquid/drunk_supply") or 0))
    end
    if liquid_type == "xx_poison" then
        me:apply_condition("xx_poison", me:query_condition("xx_poison") + (self:query("liquid/drunk_supply") or 0))
    end

    return 1
end

-- 填充液体
function liquid_base.do_fill(self, me)
    if not living(me) then
        return 1
    end
    if me:is_busy() then
        return notify_fail("你上一个动作还没有完成。")
    end

    local env = me:environment()
    if not env then
        return 0
    end

    if env:query("resource/water") == 0 then
        return notify_fail("这里没有地方可以装水。")
    end

    local liquid_name = "清水"
    local liquid_type = "water"
    local ds = 0
    local fee = 0

    local w_name = env:querystr("resource/water_name")
    local w_type = env:querystr("resource/water_type")
    if w_name and #w_name > 0 and w_type and #w_type > 0 and env:query("resource/drunk_supply") ~= 0 then
        liquid_name = w_name
        liquid_type = w_type
        ds = env:query("resource/drunk_supply")
        fee = env:query("resource/fee") or 0
    end

    if fee > 0 then
        if me:query("balance") >= fee then
            me:add_balance(-fee)
            message_vision("$N高喊道：老板，记在账上！", me)
        else
            return notify_fail("穷光蛋！不要乱凑热闹了！")
        end
    end

    if self:query("liquid/remaining") ~= 0 then
        message_vision(string.format("$N将%s里剩下的%s倒掉。", self:name(), self:querystr("liquid/name")), me)
    end

    message_vision(string.format("$N将%s装满%s。", self:name(), liquid_name), me)

    if me:is_fighting() then
        me:start_busy(2)
    end

    self:set("liquid/type", liquid_type)
    self:set("liquid/name", liquid_name)
    self:set("liquid/remaining", self:query("max_liquid"))
    self:set("liquid/drink_func", 0)
    self:set("liquid/drunk_supply", ds)

    return 1
end

-- 处理指令
function liquid_base.handle_action(self, me, comm, arg)
    if comm == "fill" then
        return liquid_base.do_fill(self, me)
    end
    return 0
end

return liquid_base

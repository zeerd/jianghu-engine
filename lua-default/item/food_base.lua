-- food_base.lua 食物/药品通用逻辑
-- 放在 lua-default 作为基础公共模块，lua-sample/item 下的食物脚本通过 require 引用

local food_base = {}

-- 是否可以使用
function food_base.Can_Use(self)
    return 1
end

-- 使用（吃食物）
function food_base.do_use(self, me, target)
    if not me then
        return 1
    end

    -- 检查食物是否还有剩余
    if self:query("food_remaining") == 0 then
        return notify_fail("%s已经没什么好吃的了。", self:name())
    end

    -- 记录当前 hp/mp，用于计算差值
    local hp = me:query("hp")
    local mp = me:query("mp")

    -- 计算恢复量
    local addhp = self:query("hp_supply")
    local addmp = self:query("mp_supply")

    if not addhp or addhp == 0 then
        addhp = math.floor(self:query("food_supply") / 3)
    end
    if not addhp or addhp <= 0 then
        addhp = 1
    end

    -- 恢复 hp/mp
    me:receive_healing("hp", addhp)
    me:receive_healing("mp", addmp)

    -- 拼装 hp/mp 变化提示
    local hpstr = ""
    local mpstr = ""

    hp = me:query("hp") - hp
    mp = me:query("mp") - mp

    if hp and hp > 0 then
        hpstr = string.format(" hp+%d", hp)
    end
    if mp and mp > 0 then
        mpstr = string.format(" mp+%d", mp)
    end

    -- 战斗中忙碌
    if me:is_fighting() then
        me:start_busy(2)
    end

    -- 减少食物，设价值为 0
    self:set("value", 0)
    self:add("food_remaining", -1)

    -- 读取消息模板
    local eat_msg = self:querystr("eat_msg")

    -- 食物吃完
    if self:query("food_remaining") <= 0 then
        if self:query("food_supply") and self:query("food_supply") > 0 then
            if eat_msg and #eat_msg > 0 then
                eat_msg = eat_msg .. "\n$n吃完了。"
            else
                eat_msg = "$N将剩下的$n吃得干干净净。"
            end
        end

        eat_msg = eat_msg .. hpstr .. mpstr
        message_vision(eat_msg, me, self)

        -- 调用 finish_eat 钩子
        -- 优先调用脚本自定义的 finish_eat（通过 Lua 方法查找链）
        -- 若脚本未定义则回退到 food_base.finish_eat（默认返回 0 = 销毁）
        local ok, result = pcall(function()
            return self:finish_eat()
        end)
        if ok and result and result ~= 0 then
            -- finish_eat 返回非 0：保留物品（如 backleg 变成骨头）
            me:SendState()
            return 1
        end

        -- 默认行为：销毁物品
        destruct(self)
        me:SendState()
        return 1
    end

    -- 未吃完
    if not eat_msg or #eat_msg == 0 then
        eat_msg = "$N拿起$n吃了几口。"
    end

    eat_msg = eat_msg .. hpstr .. mpstr
    message_vision(eat_msg, me, self)
    me:SendState()
    return 1
end

-- 吃完回调（默认返回 0 = 销毁物品）
function food_base.finish_eat(self)
    return 0
end

return food_base

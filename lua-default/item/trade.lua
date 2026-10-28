-- trade.lua  贸易文书（自动售卖机）

-- 检查某货物是否可以出售
local function check_validsell(obj)
    local base_name = obj:querystr("base_name")

    if obj:is_character() then
        return false
    end

    if base_name == "coin" then
        return false
    end
    if base_name == "trade" then
        return false
    end

    if obj:dbase_defined("no_give") then
        return false
    end
    if obj:dbase_defined("no_drop") then
        return false
    end

    -- 判断是否主人
    if obj:dbase_defined("owner") then
        return false
    end

    return true
end

-- 按 object_id 在容器中查找对象
local function find_by_id(me, nid)
    local list = me:Get_ItemList()
    if list then
        for obj in iter_items(list) do
            if obj:object_id() == nid then
                return obj
            end
        end
    end
    return nil
end

local function list_trade(self, me)
    local list = me:Get_ItemList()
    local i = 0

    for obj in iter_items(list) do
        if check_validsell(obj) then
            if obj:querystr_temp("trade/owner") ~= me:id(1) then
                obj:delete_temp("trade")
            end

            local val = obj:query_temp("trade/value")

            local msg
            if val ~= 0 then
                msg = string.format("%s ：%ld", obj:name(), val)
            else
                msg = string.format("%s ：<自用>", obj:name())
            end
            self:AddMenuItem(msg, string.format("$2value $1 %ld $2", obj:object_id()), me)
            i = i + 1
        end
    end

    if i == 0 then
        self:say("没有可以交易的物品。", me)
    end

    self:SendMenu(me)
end

return {
    -- create(self, flag)
    create = function(self, flag)
        self:set_name("贸易文书", "trade")
        self:set_weight(100)

        self:set("long", "这是官府颁发的交易许可。上面印有：公平交易，童叟无欺。")
    end,

    init = function(self)
        self:base_init()
        local me = self:environment()

        if is_user(me) then
            self:set("owner", me:id(1))
        end
    end,

    Can_Use = function(self)
        return 1
    end,

    query_autoload = function(self)
        local env = self:environment()

        if env and env:query("trade/level") ~= 0 then
            return 1
        end

        return 0
    end,

    do_use = function(self, me, target)
        if target:object_id() == me:object_id() then
            self:say(
                "请为商品定价。当对他人使用本物件时，可显示已定价格的商品清单。",
                me
            )
            list_trade(self, me)
            return 1
        end

        -- 对他人使用，表示与之交易。
        if not target:is_character() then
            return notify_fail("你要与谁交易，请看清楚点！")
        end
        if not is_living(target) then
            return notify_fail("先把对方弄醒再说。")
        end

        local who = target
        if who:is_busy() ~= 0 or who:is_fighting() ~= 0 then
            return notify_fail("对方显然没空理你。")
        end

        local list = me:Get_ItemList()
        local i = 0

        me:say(string.format("%s想卖给你货物。", me:name()), who)

        for obj in iter_items(list) do
            local val = obj:query_temp("trade/value")

            if val > 0 and obj:querystr_temp("trade/owner") == me:id(1) then
                me:AddMenuItem(
                    string.format("%s ：%ld", obj:name(), val),
                    string.format("$0buy %ld %ld", me:object_id(), obj:object_id()),
                    who
                )
                i = i + 1
            end
        end

        me:SendMenu(who)

        message_vision("$N正在向$n推销货品。", me, who)
        if i == 0 then
            message_vision("$n不高兴的对$N说：可你这也没什么好货啊。", me, who)
        end

        return 1
    end,

    handle_action = function(self, comm, me, arg)
        if comm == "value" then
            local objid = tonumber(string.match(arg, "%S+") or "0")
            local price = tonumber(string.match(arg, "%S+%s+(%S+)") or "0")

            local obj = find_by_id(me, objid)
            if not obj then
                return notify_fail("请选定商品。")
            end

            local val = price
            if val <= 0 then
                obj:delete_temp("trade")
                tell_object(me, "取消出售。")
                list_trade(self, me)
                return 1
            end

            obj:set_temp("trade/owner", me:id(1))
            obj:set_temp("trade/value", val)

            tell_object(me, "完成定价！")

            list_trade(self, me)
            return 1
        end

        return 0
    end,
}

-- poison_base.lua 毒药物品通用逻辑
-- 将原 CPoisonItem (PoisonItem.cpp) 的逻辑 Lua 化
-- 放在 lua-default 作为基础公共模块，lua-sample/item 下的毒药脚本通过 require 引用

local poison_base = {}

-- 查看物品
function poison_base.do_look(self, me)
    if not me then
        return 0
    end

    tell_object(me, string.format("\n%s(%s): %s", self:name(1), self:id(1), self:querystr("long")))
    tell_object(me, string.format("看起来它还可以被使用%d次。", self:query("index")))

    local poi = me:query_skill("poison", 1)
    if self:environment() and self:environment():object_id() == me:object_id() and poi > 0 then
        self:AddMenuItem("涂抹毒膏", "$0tumo $1", me)
        self:AddMenuItem("停止观察", "", me)
        self:SendMenu(me)
    end

    return 1
end

-- 涂毒到武器
function poison_base.do_mo(self, me)
    if self:query("index") <= 0 then
        destruct(self)
        return notify_fail("这一剂%s被用完了。", self:name(1))
    end

    if me:query_temp("poison/modu") ~= 0 then
        return notify_fail("你的武器上已经涂过%s了。", self:name(1))
    end

    local skill = me:query_skill("poison")
    if skill == 0 then
        me:apply_condition(self:querystr("poison_type"), self:query("index"))
        destruct(self)
        return notify_fail("你冒冒失失地将%s抹在了自己手上。", self:name(1))
    end

    local weapon = me:query_weapon()
    if not weapon then
        return notify_fail("你打算把%s涂在手上吗？", self:name(1))
    end

    if me:query("mp") < 30 then
        return notify_fail("你现在内力不够，无法保证自己不中毒。")
    end

    me:set_temp("poison/modu", 1)
    me:set_temp("poison/mo_type", self:querystr("poison_type"))
    me:add("mp", -30)
    me:call_out("remove_effect", skill, 0, 0)

    self:add("index", -1)
    tell_object(me, string.format("你小心翼翼地将%s涂抹到了%s上。", self:name(1), weapon:name(1)))

    return 1
end

-- 涂毒效果超时回调（call_out 注册）
function poison_base.remove_effect(self, p1, p2)
    self:delete_temp("poison/modu")
    self:SendState()
    tell_object(self, "你涂在武器上的毒药失效了。")
end

-- 处理指令
function poison_base.handle_action(self, me, comm, arg)
    if comm == "tumo" then
        return poison_base.do_mo(self, me)
    end
    if comm == "use" then
        me:apply_condition(self:querystr("poison_type"), self:query("index"))
        destruct(self)
        return 1
    end
    return 0
end

return poison_base

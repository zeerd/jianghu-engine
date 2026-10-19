-- house_rent.lua 房屋租借品规则
-- 仆役借出的物件都带 temp 标记 yongren/rent（起租物）。本模块负责判定
-- 「谁身上带着本房的租品」与「外人带着租品时哪些指令要拦下」，供
-- room/house_base.lua 的查租、赶人与命令截获复用。
--
-- Build+Run: .\run-coverage.ps1（HouseTest.*）；E2E 见 sample_house.sce

local M = {}

-- 起租物的 temp 标记：仆役借出的物件都带它
local RENT_FLAG = "yongren/rent"

-- 对象同一性：绑定层每次 push 都造新 userdata，Lua 的 == 是身份比较，
-- 判断「是不是同一个游戏对象」必须比 object_id
function M.same_object(a, b)
    if not a or not b then
        return false
    end
    return a:object_id() == b:object_id()
end

-- 物件是否为起租物
function M.is_rented(obj)
    return obj:query_temp(RENT_FLAG) ~= 0
end

-- 遍历角色身上的物件，fn 命中即返回其结果（列表为空时安全跳过）
function M.each_item(me, fn)
    local list = me:Get_ItemList()
    if not list then
        return nil
    end
    for obj in iter_items(list) do
        local hit = fn(obj)
        if hit then
            return hit
        end
    end
    return nil
end

-- 某人身上是否带着本房的租借品；房主与第二主人免查，命中返回那件物件
function M.check_rent(self, me)
    if self:querystr("owner") == me:id(1) or self:querystr("second_owner") == me:id(1) then
        return nil
    end

    return M.each_item(me, function(obj)
        if M.is_rented(obj) then
            return obj
        end
        return nil
    end)
end

-- 外人携租品时的指令截获。返回 1 表示已拦下；nil 表示放行
function M.intercept_foreign(self, comm, me, arg, tar, yongren)
    -- 截获使用时空贴的指令
    if comm == "trans" or comm == "use" then
        local obj = me:Present(tonumber(tar) or 0)
        if not obj then
            return nil
        end

        local base_name = obj:querystr("base_name")
        if base_name == "transfer_tie" or base_name == "huicheng_juanzhou" then
            if M.check_rent(self, me) then
                message_vision("$N偷偷拿出$n，结果被仆人拦住：“请把借的东西留下！”", me, obj)
                return 1
            end
        end
        return nil
    end

    -- 截获 kill / fight / cast / join / guard 指令
    if comm == "kill" or comm == "fight" or comm == "cast" or comm == "join" or comm == "guard" then
        local obj = self:Present(tonumber(tar) or 0)
        if not obj then
            return nil
        end

        if M.check_rent(self, me) or M.check_rent(self, obj) then
            message_vision("$N拦住$n道：“不行，想搞什么出去搞！”", yongren, me)
            return 1
        end
        return nil
    end

    -- 截获 drop 指令
    if comm == "drop" then
        local obj = me:Present(tonumber(tar) or 0)
        if not obj then
            return nil
        end

        if M.is_rented(obj) then
            message_vision("$N偷偷摸摸的拿出$n，想丢在地上。", me, obj)
            message_vision("$N见状喝道：想干什么！不想要的话还给我！", yongren, me)
            return 1
        end
        return nil
    end

    -- 截获 give 指令（租借品转手，交给仆役除外）
    if comm == "give" then
        local obj = me:Present(tonumber(tar) or 0)
        if not obj then
            return nil
        end

        local target = string.match(arg, "^to%s+(%d+)")
        if not target then
            return nil
        end

        if M.same_object(self:Present(tonumber(target)), yongren) then
            return nil
        end

        if M.is_rented(obj) then
            message_vision("$N偷偷摸摸的拿出$n，想转手给人。", me, obj)
            message_vision("$N见状喝道：想干什么！不想要的话还给我！", yongren, me)
            return 1
        end
        return nil
    end

    -- 截获 get 指令：租借品、带租品的人与玩家都捡不得
    if comm == "get" then
        local obj = self:Present(tonumber(tar) or 0)
        if not obj then
            return nil
        end

        if M.is_rented(obj) then
            message_vision(
                string.format("$N弯腰去拣%s，结果被$n一脚踩到了手指上！", obj:name()),
                me,
                yongren
            )
            return 1
        end

        if obj:is_character() and M.check_rent(self, obj) then
            message_vision("$N忽然拦住$n，笑道：还是算了吧！", yongren, me)
            return 1
        end

        if userp(obj) then
            message_vision("$N疑惑的问着$n：你想拣什么？", yongren, me)
            return 1
        end
        return nil
    end

    -- 截获 sell 指令
    if comm == "sell" then
        local obj = me:Present(tonumber(arg) or 0)
        if not obj then
            return nil
        end

        if M.is_rented(obj) then
            message_vision(
                string.format("$N想卖掉%s，结果被$n伸手拦住：借的东西快还！", obj:name()),
                me,
                yongren
            )
            return 1
        end
        return nil
    end

    return nil
end

return M

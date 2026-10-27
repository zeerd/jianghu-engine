-- sample_teleport.lua  传送符
-- 示例物品：展示 handle_action 的用法
-- 使用 do_use 弹出菜单，玩家选择后由 handle_action 处理命令。
--
-- 关键点：
--   1. do_use 弹出菜单（AddMenuItem + SendMenu）
--   2. handle_action 拦截命令并执行对应逻辑
--   3. 菜单命令格式 "$0<comm> $1 <arg>" 中：
--      $0 = 无输入前缀（占位符，客户端读到它才知道后面紧跟命令字）
--      $1 = 菜单所属物品自身（客户端替换为物品 ID，作为命令的目标槽，
--           命令才会被路由回本物品的 handle_action）
--      <arg> = 传给 handle_action 的固定参数

-- 将传送逻辑提取为 local 函数，再由 handle_action 直接调用。
-- 注意：不能写 self:teleport_to(...)，因为 self 是 C++ userdata，
-- 脚本自定义函数不在其 metatable 中，必须直接调用。
local function teleport_to(self, me, dest)
    local room_map = {
        ["山脚"] = "sample_mountain_foot",
        ["半山腰"] = "sample_mountain_mid",
        ["山顶"] = "sample_mountain_top",
    }

    local room_id = room_map[dest]
    if not room_id then
        return notify_fail("传送符闪烁了一下，但没有反应。")
    end

    local room = load_room(room_id)
    if not room then
        return notify_fail("传送符找不到目标地点。")
    end

    me:move(room)
    message_vision("$N激活传送符，一道白光闪过，$N消失在原地。\n", me)
    message_vision("$N出现在这里。\n", me)

    -- 传送符使用次数有限，每次传送消耗一点
    local uses = self:query("uses")
    if uses <= 1 then
        message_vision("$N手中的传送符化为灰烬，随风散去。", me)
        destruct(self)
    else
        self:set("uses", uses - 1)
    end

    return 1
end

return {
    -- create(self, nFlag)
    create = function(self, nFlag)
        self:set_name("传送符", "chuan song fu")
        self:set("unit", "张")
        self:set("long", "这是一张散发着微光的传送符，上面画着三个地点的标记。")
        self:set("value", 1000)
        self:set("no_drop", 1)
        self:set("uses", 5) -- 可使用 5 次
    end,

    -- init(self)：放在身上时，显示剩余次数
    init = function(self)
        self:base_init()
        local env = self:environment()
        if env and is_user(env) then
            self:set("owner", env:id(1))
        end
    end,

    -- do_look(self, me)：观察时显示剩余次数
    do_look = function(self, me)
        local uses = self:query("uses")
        tell_object(me, string.format("这张传送符还能使用 %d 次。", uses))
        self:AddMenuItem("使用传送符", "$0teleport $1", me)
        self:SendMenu(me)
        return 1
    end,

    -- handle_action(self, comm, me, arg)
    -- 当玩家从菜单选择"使用传送符"时，引擎调用此函数。
    -- comm = "teleport"，arg = ""（由菜单格式 "$0teleport $1" 决定）
    handle_action = function(self, comm, me, arg)
        if comm == "teleport" then
            -- 弹出二级菜单，让玩家选择目的地
            -- 必须写成 "$0dest $1 <地点>"：$0 前缀不触发输入，$1 由客户端替换为
            -- 本物品 ID 作为目标槽，命令才会路由回本物品；否则会退化成向玩家
            -- 索取目标编号。
            self:AddMenuItem("传送至「山脚」", "$0dest $1 山脚", me)
            self:AddMenuItem("传送至「半山腰」", "$0dest $1 半山腰", me)
            self:AddMenuItem("传送至「山顶」", "$0dest $1 山顶", me)
            self:AddMenuItem("取消", "", me)
            self:SendMenu(me)
            return 1
        end

        if comm == "dest" then
            return teleport_to(self, me, arg)
        end

        return 0
    end,
}

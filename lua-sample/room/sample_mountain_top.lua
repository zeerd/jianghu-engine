-- sample_mountain_top.lua  青云山山顶（副本入口）
-- 示例房间：展示副本入口房间的简单实现
-- 使用 require 引入房间公共模块，复用 do_look 逻辑

local common = require("room.sample_room_common")

local M = {}

M.type = "room"
M.name = "青云山山顶"

M.create = function(self)
    self:set_name("青云山山顶")
    self:add_door("青云山半山腰", "青云山半山腰", "青云山山顶")
    self:set("monster_chance", 30)

    self:set(
        "long",
        "这里是青云山的山顶，可以俯瞰整个山谷。前方隐约可见一座山寨，那是山贼的老巢。"
    )
end

M.do_look = function(self, me)
    -- 只复用公共模块的描述显示，不复用 do_look（因为需要追加自定义菜单项）
    common.say_long(self, me)
    self:say("山顶前方有一座山寨，你可以选择闯入。", me)
    self:AddMenuItem("闯入山寨", "$0enter_fb $1", me)
    self:SendMenu(me)
    return 1
end

M.handle_action = function(self, comm, me, arg, tar)
    if comm == "enter_fb" then
        return M.do_enter_fb(self, me, arg)
    end
    return 0
end

M.do_enter_fb = function(self, me, arg)
    local fb = load_fb("sample_stronghold_fb")
    fb:Begin_FB("青云山山顶", 3600, 1)
    return fb:MoveIn(me)
end

return M

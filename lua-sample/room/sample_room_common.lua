-- sample_room_common.lua  山脚系房间公共逻辑
-- 示例共享模块：展示 require 的用法
-- 三个山脚房间（山脚、半山腰、山顶）共用此模块的 do_look 函数。
-- 用法：local common = require("room.sample_room_common")

local M = {}

-- 仅显示房间描述（不弹菜单），供需要追加自定义菜单项的房间复用
function M.say_long(self, me)
    self:say(self:querystr("long"), me)
end

-- 通用的 do_look 实现：显示房间描述并弹出菜单
function M.do_look(self, me)
    M.say_long(self, me)
    self:SendMenu(me)
    return 1
end

return M

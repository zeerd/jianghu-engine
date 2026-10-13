-- sample_mountain_foot.lua  青云山山脚
-- 示例房间：展示普通房间的简单实现
-- 使用 require 引入房间公共模块，复用 do_look 逻辑

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "青云山山脚",

    create = function(self)
        self:set_name("青云山山脚")
        self:add_npc("sample_guide")
        self:add_npc("sample_vendor")
        self:add_door("青云山半山腰", "青云山半山腰", "青云山山脚")
        self:add_door("山脚茶棚", "山脚茶棚", "青云山山脚")
        self:set("no_fight", 1)

        -- 放置箱子
        local box = self:Add_Box()
        box:add_object("sample_sword_basic", 1)
        box:add_object("coin", 60000)
        box:set_name("木箱")

        local box2 = self:Add_Box()
        box2:add_object("sample_heal", 3)
        box2:set_name("药箱")

        self:set(
            "long",
            "这里是青云山的山脚，一条蜿蜒的山路通向山顶。路边有一个杂货铺，还有一位老者在为路人指路。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}

-- sample_mountain_mid.lua  青云山半山腰
-- 示例房间：展示普通房间的简单实现
-- 使用 require 引入房间公共模块，复用 do_look 逻辑

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "青云山半山腰",

    create = function(self)
        self:set_name("青云山半山腰")
        self:add_npc("sample_master")
        self:add_npc("sample_wooden_dummy")
        self:add_door("青云山山脚", "青云山山脚", "青云山半山腰")
        self:add_door("青云山山顶", "青云山山顶", "青云山半山腰")

        self:set(
            "long",
            "这里是青云山的半山腰，云雾缭绕，风景宜人。一位隐世高手在此隐居，据说可以传授武功。路边有几间草棚可供旅人临时休息，另立着一个供人练功的木人。"
        )
        self:set("valid_startroom", 1)
        self:set("sleep_room", 1)
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}

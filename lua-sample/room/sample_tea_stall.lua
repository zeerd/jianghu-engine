-- sample_tea_stall.lua  山脚茶棚
-- 示例房间：展示 resource/water（fill 装水）和商人 NPC 的用法
return {
    type = "room",
    name = "山脚茶棚",

    create = function(self)
        self:set_name("山脚茶棚")

        -- 支持 fill 命令的关键：标记此房间有水源
        self:set("resource/water", 1)
        self:set("resource/water_name", "清茶")
        self:set("resource/water_type", "tea")
        -- 茶棚是营业场所，装一壶茶收 10 文茶钱（fee>0 时 liquid_base.do_fill
        -- 从客人的钱庄存款扣费，分文没有的客人装不了水）
        self:set("resource/fee", 10)

        -- 连接回山脚
        self:add_door("青云山山脚", "青云山山脚", "山脚茶棚")

        self:set(
            "long",
            "路边搭着一个简陋的茶棚，几根竹竿撑起一片油布，下面摆着两张条凳和一张矮桌。"
                .. "炉上的水壶正咕嘟作响，空气中弥漫着淡淡的茶香。"
        )
    end,

    do_look = function(self, me)
        self:say(self:querystr("long"), me)
        self:say("这里可以给容器装水（fill <容器ID>）。", me)
        self:SendMenu(me)
        return 1
    end,
}

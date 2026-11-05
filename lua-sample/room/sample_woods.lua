-- sample_woods.lua  山脚林间空地
-- 示例房间：展示「死胡同」式的支线场地——只从山脚开一扇门、没有第二条路，
-- 进来的只有专程来访的人。房间本身不设 no_fight，动手全看屋里的人。
-- 山道强人（npc/sample_road_rogue.lua）驻扎于此：记仇、动手、销账、死亡
-- 这条战斗状态链都在这间屋里演，屋里没有第二个人，播报不会串到别处。

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "山脚林间空地",

    create = function(self)
        self:set_name("山脚林间空地")
        self:add_npc("sample_road_rogue")
        -- 双向开门：山脚那一侧的门由 room/sample_mountain_foot.lua 对称声明
        self:add_door("青云山山脚", "青云山山脚", "山脚林间空地")
        self:set(
            "long",
            "山路在此分出一条岔口，林木掩着一片空地，地上散着折断的草茎"
                .. "与几枚崩口的铁蒺藜——看得出常有人在这里拦路动手。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}

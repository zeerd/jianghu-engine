-- sample_arena.lua  山脚演武场
-- 示例房间：展示「脚本化比武」的场地——战斗房（fight_room）是引擎按需
-- 创建的临时空间（do_bishi/kill 同款），本场只是教头开比的落脚点：
-- 玩家与教头都会被挪进载入的战斗房，比完自动回到这里。
-- 与 sample_wooden_dummy（对练）、sample_iron_dummy（喂招）的分工：
-- 这里是「有裁决的比试」——报战况、点到为止、判输赢。

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "山脚演武场",

    create = function(self)
        self:set_name("山脚演武场")
        self:add_npc("sample_arena_master")
        self:add_door("青云山山脚", "青云山山脚", "山脚演武场")

        self:set(
            "long",
            "山脚西侧一片夯实的黄土场，四周立着兵器架与草靶，"
                .. "地上画着比试用的圆圈。一位老教头守在这里，"
                .. "谁想活动筋骨，找他 /ask 比武便是。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}

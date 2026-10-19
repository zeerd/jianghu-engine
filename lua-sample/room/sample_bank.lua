-- sample_bank.lua  山脚钱庄
-- 示例房间：展示 type="bankowner" 钱庄老板 NPC 的用法
-- 玩家可对钱庄老板执行 deposit/withdraw/balance/loan 命令。

local common = require("room.sample_room_common")

return {
    type = "room",
    name = "山脚钱庄",

    create = function(self)
        self:set_name("山脚钱庄")
        self:add_npc("sample_banker")
        self:add_door("青云山山脚", "青云山山脚", "山脚钱庄")
        self:set("no_fight", 1)

        self:set(
            "long",
            "一间不大的钱庄，门口挂着「童叟无欺」的匾额。"
                .. "柜台后坐着一位老掌柜，正拨弄着算盘。"
        )
    end,

    do_look = function(self, me)
        return common.do_look(self, me)
    end,
}

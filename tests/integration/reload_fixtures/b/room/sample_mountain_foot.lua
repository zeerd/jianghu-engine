-- sample_mountain_foot.lua  热重载 E2E 起始房间（fixture，变体 b）
local common = require("room.e2e_reload_common")

return {
    type = "room",
    name = "青云山山脚",

    create = function(self)
        self:set_name("青云山山脚")
        self:add_npc("e2e_reload_npc")

        local box = self:Add_Box()
        box:add_object("e2e_reload_item", 3)
        box:set_name("重载箱", "zai xiang")

        self:set("long", "【长描述乙】这里是青云山的山脚。")
        self:set("no_fight", 1)
    end,

    -- reset_runs / 缓存 探针：见 a 变体头注
    reset = function(self)
        local n = (self:query("reset_runs") or 0) + 1
        self:set("reset_runs", n)
        local cached = package.loaded["room.e2e_reload_common"]
        local v = (type(cached) == "table" and cached.version) or "nil"
        self:set(
            "long",
            "【长描述乙】这里是青云山的山脚。（reset×" .. n .. "，缓存=" .. tostring(v) .. "）"
        )
    end,

    do_look = function(self, me)
        self:say("【房景乙】", me)
        return common.do_look(self, me)
    end,
}

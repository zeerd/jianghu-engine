-- sample_mountain_foot.lua  热重载 E2E 起始房间（fixture，变体 a）
-- 顶替私有脚本副本中的同名教材房间：登录即达（config start_room 不变），
-- 剧本全程无需行走。A/B 变体由剧本 filecopy 切换。
local common = require("room.e2e_reload_common")

return {
    type = "room",
    name = "青云山山脚",

    create = function(self)
        self:set_name("青云山山脚")
        self:add_npc("e2e_reload_npc")

        -- 重载药放在箱里：取物走 look 菜单（与教材木箱同款链路）
        local box = self:Add_Box()
        box:add_object("e2e_reload_item", 3)
        box:set_name("重载箱", "zai xiang")

        self:set("long", "【长描述甲】这里是青云山的山脚。")
        self:set("no_fight", 1)
    end,

    -- 重载桥只在脚本定义 reset(self) 时回调（LuaRoom.inl）：借它把 create()
    -- 写下的 dbase 状态（long）刷新到新版本。函数闭包（do_look 的房景标记）
    -- 换 ref 即生效，不依赖 reset——两个刷新通道各验一头。
    -- reset_runs 是重载探针：每次 reset 自增并写进 long，look 第二句即可
    -- 读出「实例被通知了几次」——区分「通知没到」与「require 拿到旧模块」。
    -- 缓存= 直接读 package.loaded 里共享模块的 version 字段：与 do_look 的
    -- 共甲/乙标记对照，立刻定位「清缓存没生效」还是「require 读了别的文件」。
    reset = function(self)
        local n = (self:query("reset_runs") or 0) + 1
        self:set("reset_runs", n)
        local cached = package.loaded["room.e2e_reload_common"]
        local v = (type(cached) == "table" and cached.version) or "nil"
        self:set(
            "long",
            "【长描述甲】这里是青云山的山脚。（reset×" .. n .. "，缓存=" .. tostring(v) .. "）"
        )
    end,

    do_look = function(self, me)
        self:say("【房景甲】", me)
        return common.do_look(self, me)
    end,
}

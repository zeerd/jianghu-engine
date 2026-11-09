-- rk.lua  游戏大厅

-- 双身份：既作为全局大厅房被直接加载，也作为 rk_fb 副本实例的一局被
-- create_fb_room 实例化（base_name 恒为 rk，故两处行为完全一致）。

-- 注册专员的命令白名单：talk 是与专员交谈，regxxx 是注册流程
-- （菜单项回灌的命令也会先经过房间拦截，必须放行）。
local allow = {
    talk = true,
    regchar = true,
    regname = true,
    regpass = true,
    regemail = true,
    reggender = true,
    regstat = true,
    regdone = true,
}

return {
    type = "room",
    name = "rk",

    create = function(self)
        self:set_name("游戏大厅")
        self:set("wiz_room", 1)

        -- 角色注册专员：负责在这里创建新角色
        self:add_npc("register_clerk")
    end,

    handle_action = function(self, comm, me, arg, target)
        if allow[comm] then
            return 0
        end

        if comm ~= "quit" and comm ~= "mudage" and comm ~= "password" and comm ~= "validate" then
            tell_object(me, "你尝试了一下，但是发现这里似乎没有信号。")
            return 1
        end
        return 0
    end,
}

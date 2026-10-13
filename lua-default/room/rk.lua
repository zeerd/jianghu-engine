-- wiz_rk.lua  游戏大厅

-- 注册专员的命令白名单：talk 是与专员交谈，register/regxxx 是注册流程
-- （菜单项回灌的命令也会先经过房间拦截，必须放行）。
local allow = {
    talk = true,
    register = true,
    regchar = true,
    regname = true,
    regpass = true,
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
        -- register 快捷命令：省去查编号，直接开始注册流程。
        -- 只转发 ASCII 的 regchar，带文字的命令（regname 等）一律回 0，
        -- 交给命令分发器的目标截获环节处理（那里的 arg 是原始 GBK，
        -- 而房间钩子里的 arg 已被转成 UTF-8，不能直接转发给 NPC）。
        if comm == "register" then
            local who = nil
            local objs = self:Get_ItemList()
            if objs then
                for ob in iter_items(objs) do
                    if ob:querystr("base_name") == "register_clerk" then
                        who = ob
                        break
                    end
                end
            end
            if who == nil then
                tell_object(me, "这里没有注册专员。")
                return 1
            end
            return handle_action(who, "regchar", me, "")
        end

        if allow[comm] then
            return 0
        end

        if
            comm ~= "card"
            and comm ~= "createcard"
            and comm ~= "addcard"
            and comm ~= "removecard"
            and comm ~= "showcards"
            and comm ~= "quit"
            and comm ~= "mudage"
            and comm ~= "password"
            and comm ~= "validate"
        then
            tell_object(
                me,
                "你尝试了一下，但是发现这里似乎没有信号。"
                    .. "（请输入/card命令获取进一步指示，输入register创建角色）"
            )
            return 1
        end
        return 0
    end,
}

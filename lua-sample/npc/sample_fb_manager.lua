-- sample_fb_manager.lua  副本管理员（示例）
-- 示例NPC：展示副本管理员的简单实现，提供离开副本功能

local npc = {}

function npc.create(self)
    self:set_name("小头目", "xiao tou mu")
    self:set("title", "副本管理员")

    self:set("gender", "男性")
    self:set("age", 40)
    self:set("long", "这是副本管理员，可以通过谈话，选择相应功能。")
    self:set("attitude", "friendly")

    self:set("no_kill", 1)
    self:set("no_talk", 1)

    self:set("combat_exp", 100000)

    self:set("max_hp", 50000)
    self:set("hp", 50000)
    self:set("max_mp", 10000)
    self:set("mp", 10000)
end

function npc.do_talk(self, me, ask)
    self:say("有什么可以帮助你的吗？", me)
    self:AddMenuItem("离开副本", "$0leave $1", me)
    self:SendMenu(me)
    return 1
end

function npc.handle_action(self, comm, me, arg)
    if comm == "leave" then
        return npc.do_leave(self, me)
    elseif comm == "kill" or comm == "fight" or comm == "cast" then
        tell_object(me, "$HIY唉，现在真是世风日下，连我这糟老头子也想杀。")
        return 1
    end
    -- 返回 0（未处理）让引擎继续走常规命令分发（如 talk 走 do_talk）；
    -- 返回 -1 在 C++ 侧是「真值」，会把命令吞掉，导致 talk 无法触发 do_talk。
    return 0
end

function npc.do_leave(self, me)
    local r = self:environment()
    if not r or not r:query_fb_parent() then
        message_vision("我这是在哪呢，我都不知道身处何方，又如何为你指路。", me)
        return 0
    end

    local env = nil
    local sm = r:query_fb_parent():querystr("source_map")
    if sm and sm ~= "" then
        env = load_room(sm)
    else
        env = load_room(START_ROOM)
    end

    message_vision(
        "$N用手一划，出现了一扇烟雾弥漫的门，$n未假思索，从这扇门走了出去。",
        self,
        me
    )
    if env then
        me:move(env)
    end
    return 1
end

return npc

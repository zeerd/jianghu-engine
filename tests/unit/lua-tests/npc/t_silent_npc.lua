-- t_silent_npc.lua  播报类回调一律返回 0 的 NPC 夹具
-- 引擎侧必须同样把 0 当「无消息」，而不是转成文本 "0" 播出去。
local npc = {}

function npc.create(self)
    self:set_name("闷声葫芦", "silent npc")
end

function npc.chat_msg(self)
    return 0
end

function npc.chat_msg_combat(self)
    return 0
end

function npc.hit_ob(self, me, victim, damage_bonus)
    return 0
end

return npc

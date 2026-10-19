-- t_fixture_npc.lua  覆盖用 NPC 夹具
-- 供 tests/test_lua_npc.cpp 驱动 CLuaNpcT 的各个虚函数：脚本把每次调用
-- 记进 self 的 dbase，C++ 侧据此断言调用确实到达脚本。
local npc = {}

function npc.create(self)
    self:set_name("夹具伙计", "fixture npc")
    self:set("long", "一个专给单元测试使唤的伙计。")
    self:set("hp", 100)
    self:set("max_hp", 100)
end

function npc.setup(self)
    self:set("setup_done", 1)
end

function npc.do_talk(self, me, ask)
    self:set("last_talk", ask or "")
    return 1
end

function npc.handle_action(self, me, comm, arg)
    self:set("last_action", comm or "")
    return 0
end

function npc.handle_action4(self, me, comm, arg, tar)
    self:set("last_action", comm or "")
    return 0
end

function npc.accept_object(self, me, item)
    self:set("last_accepted", 1)
    return 1
end

function npc.init(self, me)
    self:set("init_done", 1)
end

function npc.die(self)
    self:set("last_died", 1)
end

function npc.finish_kill(self, me)
    self:set("last_finish_kill", 1)
end

function npc.is_defeated(self, winner, failer)
    self:set("last_defeated", 1)
end

function npc.do_look(self, me)
    self:set("last_look", 1)
    return 1
end

function npc.hit_ob(self, me, victim, damage_bonus)
    self:set("last_hit_bonus", damage_bonus)
    return "你被拍了一下。"
end

function npc.chat_msg(self)
    self:set("last_chat", 1)
    return "夹具伙计嘀咕了一句。"
end

function npc.chat_msg_combat(self)
    self:set("last_combat_chat", 1)
    return "夹具伙计抡起了扫帚。"
end

function npc.greeting(self, me)
    self:set("last_greeting", 1)
    return "客官里边儿请。"
end

function npc.attempt_apprentice(self, me)
    self:set("last_attempt_apprentice", 1)
end

function npc.prevent_learn(self, me, skill)
    self:set("last_prevent_learn", skill or "")
    return 0
end

function npc.recognize_apprentice(self, me)
    self:set("last_recognize", 1)
    return 1
end

function npc.accept_fight(self, me)
    self:set("last_accept_fight", 1)
    return 1
end

function npc.attack(self, opponent)
    self:set("last_attack", 1)
end

function npc.relay_say(self, me, msg)
    self:set("last_relay_say", msg or "")
end

function npc.relay_emote(self, me, verb)
    self:set("last_relay_emote", verb or "")
end

function npc.answer_origin(self, me)
    return "我本是后山一块石头。"
end

return npc

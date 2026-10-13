-- t_error_npc.lua  测试夹具：每个回调都抛错的 NPC
-- 供 tests/test_lua_error_paths.cpp 验证 CLuaNpcT 的报错契约：
-- 脚本回调抛错时，包装层必须打一条标明脚本名与回调名的 ERROR 日志，
-- 并按各自语义回退基类/静默吞掉，绝不让 Lua 错误穿透到 C++ 调用方。
local npc = {}

function npc.create(self)
    self:set_name("报错伙计", "error npc")
    self:set("long", "一个专门用来演示脚本报错的伙计。")
    self:set("hp", 100)
    self:set("max_hp", 100)
end

function npc.setup(self)
    error("setup boom")
end

function npc.reload(self)
    error("reload boom")
end

function npc.do_talk(self, me, ask)
    error("do_talk boom")
end

function npc.handle_action(self, me, comm, arg)
    error("handle_action boom")
end

function npc.handle_action4(self, me, comm, arg, tar)
    error("handle_action4 boom")
end

function npc.accept_object(self, me, item)
    error("accept_object boom")
end

function npc.init(self, me)
    error("init boom")
end

function npc.die(self)
    error("die boom")
end

function npc.finish_kill(self, me)
    error("finish_kill boom")
end

function npc.is_defeated(self, winner, failer)
    error("is_defeated boom")
end

function npc.do_look(self, me)
    error("do_look boom")
end

function npc.hit_ob(self, me, victim, damage_bonus)
    error("hit_ob boom")
end

function npc.chat_msg(self)
    error("chat_msg boom")
end

function npc.chat_msg_combat(self)
    error("chat_msg_combat boom")
end

function npc.greeting(self, me)
    error("greeting boom")
end

function npc.attempt_apprentice(self, me)
    error("attempt_apprentice boom")
end

function npc.prevent_learn(self, me, skill)
    error("prevent_learn boom")
end

function npc.recognize_apprentice(self, me)
    error("recognize_apprentice boom")
end

function npc.accept_fight(self, me)
    error("accept_fight boom")
end

function npc.attack(self, opponent)
    error("attack boom")
end

function npc.relay_say(self, me, msg)
    error("relay_say boom")
end

function npc.relay_emote(self, me, verb)
    error("relay_emote boom")
end

-- 询问回调：由 set_inquiry_func 注册，do_talk 回退到询问逻辑时触发
function npc.boom_inquiry(self, me)
    error("inquiry boom")
end

return npc

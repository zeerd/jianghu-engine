-- t_quiet_npc.lua  不定义 do_talk 的 NPC 夹具
-- 供 tests/test_lua_npc.cpp 走 CLuaNpcT::do_talk 的回退路径：
-- 脚本没有 do_talk 时交给 set_inquiry 注册的询问逻辑处理。
local npc = {}

function npc.create(self)
    self:set_name("闷葫芦", "quiet npc")
end

function npc.answer_origin(self, me)
    return "我本是后山一块石头。"
end

function npc.answer_silent(self, me)
    self:say("我自个儿说完了。", me)
    self:send_menu(me)
    return 0
end

return npc

-- t_callout_issuer.lua  测试专用：call_out 发起脚本存活性夹具
--
-- 回调定义在本脚本（发起脚本）里、注册到别的对象身上；发起的 NPC 实例销毁后，
-- 脚本表仍由引擎脚本缓存持有，到点应照常执行且 self = 注册对象。
-- 用例见 tests/unit/test_lua_callout.cpp（IssuerDestroyedCallbackStillFiresOnTarget）。
local npc = {}

function npc.create(self)
    self:set_name("发起方夹具", "fa qi fang")
end

function npc.test_issuer_cb(self, p1, p2)
    callout_test_ran = 1
    self:set("issuer_cb_mark", 1)
end

return npc

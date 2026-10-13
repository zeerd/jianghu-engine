-- quest_mgr.lua  测试夹具：四个事件回调全部抛错的任务模块
-- 供 tests/test_lua_error_paths.cpp 验证 LuaQuest.cpp 的报错契约。
-- 模块由 require("quest.quest_mgr") 取用，任务系统本身允许缺席，
-- 因此本夹具只在测试进程内存在，不影响游戏数据集。
local M = {}

function M.has_quest(me)
    error("has_quest boom")
end

function M.on_give_item(npc, me, item)
    error("on_give_item boom")
end

function M.on_talk(npc, me, ask)
    error("on_talk boom")
end

function M.on_npc_killed(killer, victim)
    error("on_npc_killed boom")
end

return M

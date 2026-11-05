-- e2e_reload_common.lua  热重载 E2E 专用共享模块（fixture，变体 a）
-- 被起始房间 require：验证 package.loaded 缓存语义——单个 reload 与自动
-- 重载都不清 require 缓存（共享模块行为保持旧版），reload all 才换。
-- version 是诊断探针：房间 do_look 的共甲/乙标记直接由它渲染，重载次数
-- 与 package.loaded 缓存值由房间 reset 写进 long，一眼钉住「require 拿到
-- 了哪个版本」。
local M = {}

M.version = "甲"

function M.do_look(self, me)
    self:say(self:querystr("long"), me)
    self:say("【共" .. M.version .. "】", me)
    return 1
end

return M

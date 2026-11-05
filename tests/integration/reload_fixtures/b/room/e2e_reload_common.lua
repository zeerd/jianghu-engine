-- e2e_reload_common.lua  热重载 E2E 专用共享模块（fixture，变体 b）
-- version 探针见 a 变体头注。
local M = {}

M.version = "乙"

function M.do_look(self, me)
    self:say(self:querystr("long"), me)
    self:say("【共" .. M.version .. "】", me)
    return 1
end

return M

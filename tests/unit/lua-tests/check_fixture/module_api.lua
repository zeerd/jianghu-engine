-- module_api.lua: module surface for the module-field check (defense 9).
local M = {}

function M.ping() end

M.pong = function() end

local function hidden() end

return M

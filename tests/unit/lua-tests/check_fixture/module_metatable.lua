-- module_metatable.lua: the returned table carries a metatable, so fields may
-- come from __index and the export surface is not statically decidable.
local M = {}

function M.base() end

setmetatable(M, { __index = {} })

return M

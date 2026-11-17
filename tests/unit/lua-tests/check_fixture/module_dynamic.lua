-- module_dynamic.lua: keys are computed at runtime, so the export surface is
-- not statically decidable and the check must stay quiet for this module.
local M = {}

for _, name in ipairs({ "one", "two" }) do
    M[name] = function() end
end

return M

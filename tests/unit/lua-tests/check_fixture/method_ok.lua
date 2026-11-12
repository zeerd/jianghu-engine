-- check-lua fixture: a script-defined method resolves through its own table,
-- and an engine-bound method resolves through self — both must not be flagged.
local M = {}

function M.helper(self)
    return 1
end

local t = {}

return function(self)
    M.helper(self)
    t:helper()
    self:set_name("a", "b")
end

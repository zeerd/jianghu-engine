-- check-lua fixture: script-defined and engine-bound methods are both
-- legitimate ':' targets and must not be flagged.
local M = {}

function M.helper(self)
    return 1
end

return function(self)
    self:helper()
    self:set_name("a", "b")
end

-- check-lua fixture: self is the engine-pushed object userdata, so a method
-- that exists only in a script table is unreachable through it — the call
-- raises "attempt to call a nil value" at runtime no matter who defines it.
local M = {}

function M.helper(self)
    return 1
end

return function(self)
    return self:helper()
end

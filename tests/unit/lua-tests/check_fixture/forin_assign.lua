-- Fixture: a global assigned INSIDE a `for ... in` body is still a global
-- definition (statement level, not a table field).
local list = { 1 }
for k, v in ipairs(list) do
    in_loop_total = v
end

return true

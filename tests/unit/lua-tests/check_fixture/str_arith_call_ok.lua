-- Fixture: in f("a") + 1 the string literal is an ARGUMENT, not an operand.
-- That is legal Lua and must not be swept up by the string-arithmetic check.
local function double(x)
    return #x
end

local value = double("a") + 1

return value

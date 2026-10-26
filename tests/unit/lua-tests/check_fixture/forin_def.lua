-- Fixture: globals defined AFTER a `for ... in` loop must still be registered
-- as definitions. Regression: the loop-header scan only stopped at an LT_OP
-- `=`, and `in` is a keyword (LT_KW), so it ran on and swallowed every name
-- up to the next `=` into `locals` -- these two definitions were lost and the
-- reader in forin_read.lua was reported as reading unbound globals.
local list = { 1, 2 }
for k, v in ipairs(list) do
    print(v)
end

function forin_helper(x)
    return x
end

forin_flag = true

return true

-- Fixture: parentheses do not change operand types -- ("a") + 1 is exactly as
-- invalid as "a" + 1, and must be reported.
local bad = ("alpha") + ("beta")

return bad

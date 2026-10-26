-- check-lua fixture: string literal in arithmetic must be flagged.
-- Lua has no '+' for strings; this compiles fine and explodes at runtime.
local bad = "alpha" + "beta"
return { bad = bad }

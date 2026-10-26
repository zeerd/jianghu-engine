-- check-lua fixture: '..' is the string concatenation operator, never an
-- error -- guards against the arithmetic check flagging every string.
local a = "alpha"
local b = "beta"
local c = a .. b
local d = "gamma" .. c
return { c = c, d = d }

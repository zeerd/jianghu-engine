-- check-lua fixture: single-argument math.random(0) does not return 0 (Lua 5.5
-- wraps the empty interval into a full-range value).
local r = math.random(0)
return { r = r }

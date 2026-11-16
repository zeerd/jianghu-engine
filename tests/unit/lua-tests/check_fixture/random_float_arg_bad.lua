-- check-lua fixture: random() takes luaL_checkinteger, and mp / 2 is a float
-- with no integer representation whenever mp is odd.
local mp = 101
local half = random(mp / 2)
return { half = half }

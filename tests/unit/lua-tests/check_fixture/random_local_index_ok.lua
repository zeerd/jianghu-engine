-- check-lua fixture: offset applied at assignment keeps the index 1-based.
local actions = { { lvl = 0 }, { lvl = 15 } }
local i = 2
local idx = random(i) + 1
local a = actions[idx]
return { a = a }

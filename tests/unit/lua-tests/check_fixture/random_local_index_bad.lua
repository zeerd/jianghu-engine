-- check-lua fixture: bare random() result stored in a local, then used as index.
local actions = { { lvl = 0 }, { lvl = 15 } }
local i = 2
local idx = random(i)
local a = actions[idx]
return { a = a }

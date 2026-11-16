-- check-lua fixture: engine random() is 0-based, so t[random(n)] can hit t[0] (= nil).
local actions = { "a", "b", "c" }
local i = 3
local pick = actions[random(i)]
return { pick = pick }

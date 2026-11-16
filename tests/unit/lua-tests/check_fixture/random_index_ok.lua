-- check-lua fixture: 0-based random needs +1; math.random is 1-based and needs none.
local actions = { "a", "b", "c" }
local i = 3
local pick = actions[random(i) + 1]
local pick2 = actions[math.random(i)]
return { pick = pick, pick2 = pick2 }

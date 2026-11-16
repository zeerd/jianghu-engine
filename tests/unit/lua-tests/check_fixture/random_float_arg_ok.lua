-- check-lua fixture: //, math.floor and plain integers are all valid arguments.
local mp = 101
local a = random(mp // 2)
local b = random(math.floor(mp / 2))
local c = random(mp)
local d = math.random(mp // 2)
return { a = a, b = b, c = c, d = d }

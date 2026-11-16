-- check-lua fixture: a same-named local in another function is a different
-- variable; the random() assignment in chat() must not flag pick()'s index.
local mon = { "a", "b" }

local function pick()
    local i = 1
    return mon[i]
end

local function chat()
    local i = random(4)
    if i == 0 then
        return "first"
    end
    return "other"
end

return { pick = pick, chat = chat }

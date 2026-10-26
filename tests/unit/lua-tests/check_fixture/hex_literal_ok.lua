-- check-lua fixture: hex literals are numbers, not code. A lexer that stops
-- at '0' leaves 'x41' behind and reports it as an unbound global.
local function is_letter(b)
    return (b >= 0x41 and b <= 0x5A) or (b >= 0x61 and b <= 0x7A)
end

local function in_range(b)
    return b > 0x20 and b < 0xFF and b ~= 0xA0
end

local e = 1e3
return { letter = is_letter(65), range = in_range(100), e = e }

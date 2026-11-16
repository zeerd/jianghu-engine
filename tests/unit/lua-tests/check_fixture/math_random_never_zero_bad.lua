-- check-lua fixture: math.random(n) is at least 1, so == 0 is never true.
local hit = 0
if math.random(3) == 0 then
    hit = 1
end
return { hit = hit }

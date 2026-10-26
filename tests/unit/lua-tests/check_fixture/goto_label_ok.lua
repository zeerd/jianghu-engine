-- check-lua fixture: a goto label is a jump target, not a global read.
-- Regression: label names were once reported as unbound globals.
local out = {}
for i = 1, 3 do
    if i == 2 then goto skip end
    out[#out + 1] = i
    ::skip::
end
return out

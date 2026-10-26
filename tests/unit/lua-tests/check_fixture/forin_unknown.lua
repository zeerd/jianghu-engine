-- Fixture: an unknown global read right after `for ... in` must still be
-- reported -- the old header scan swallowed the whole window into `locals`
-- and the read went unchecked.
local t = { 1 }
for i, v in ipairs(t) do
    unknown_after_forin_xyz(v)
end

return true

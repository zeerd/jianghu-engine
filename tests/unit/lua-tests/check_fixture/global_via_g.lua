-- Fixture: `_G.name = v` and `rawset(_G, "name", v)` are just other spellings
-- of a global definition; readers in another file must resolve.
_G.g_via_g = 1
rawset(_G, "g_rawset", 2)

return true

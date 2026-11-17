-- module_field_metatable_ok.lua: fields may arrive through __index, so the
-- check must stay quiet for this module.
local m = require("check_fixture.module_metatable")

m.whatever()

return { ok = true }

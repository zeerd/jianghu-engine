-- module_field_table_bad.lua: the module returns a constructor that has no
-- gamma key.
local t = require("check_fixture.module_table")

t.gamma()

return { ok = true }

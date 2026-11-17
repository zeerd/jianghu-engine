-- module_field_direct_ok.lua: field call chained straight onto require().
require("check_fixture.module_api").ping()

return { ok = true }

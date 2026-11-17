-- module_field_direct_bad.lua: chained call on a field the module never
-- exports.
require("check_fixture.module_api").gone()

return { ok = true }

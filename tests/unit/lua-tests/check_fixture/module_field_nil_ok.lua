-- module_field_nil_ok.lua: the module returns no table, so there is no export
-- surface to reconcile against. Bound through the parenthesis-free require form.
local n = require "check_fixture.module_nil"

n.whatever()

return { ok = true }

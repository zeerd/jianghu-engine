-- module_field_guard_ok.lua: the field is probed before it is called, so the
-- call site is intentional even though the module never exports it.
local api = require("check_fixture.module_api")

if api.optional then
    api.optional()
end

return { ok = true }

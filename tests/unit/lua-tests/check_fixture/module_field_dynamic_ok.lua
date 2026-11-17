-- module_field_dynamic_ok.lua: the module builds its keys at runtime, so the
-- check cannot know its surface and must not report.
local d = require("check_fixture.module_dynamic")

d.whatever()

return { ok = true }

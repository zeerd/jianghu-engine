-- module_field_ok.lua: calling an exported field must pass.
local api = require("check_fixture.module_api")

api.ping()

return { ok = true }

-- module_field_missing_bad.lua: ping/pong are exported, gone and absent are not.
local api = require("check_fixture.module_api")

api.gone()
api.absent()

return { ok = true }

-- module_field_reassign_ok.lua: the local stops referring to the module, so
-- field calls on it are out of the check's scope.
local api = require("check_fixture.module_api")

api = { other = 1 }
api.gone()

return { ok = true }

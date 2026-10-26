-- Fixture: require("check_fixture." .. "helper") has no literal module name.
-- It must be counted as a dynamic require (NOTE, not a failure): checking the
-- literal prefix "check_fixture." would be a false failure.
local helper = require("check_fixture." .. "helper")

return { ok = helper ~= nil }

-- Fixture: `require "mod"` (no parentheses) is still a literal require and has
-- to be resolved; skipping it silently would hide a missing module.
local missing = require "check_fixture.no_such_module_noparen"

return { m = missing }

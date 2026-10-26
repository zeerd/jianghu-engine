-- Fixture: file-handle methods (seek/setvbuf, ...) live on the FILE metatable,
-- not in the `io` table, so a colon call on a handle must not be reported as
-- unbound. The checker reads the metatable of a real handle to build that part
-- of its whitelist. (This file is only compiled/lexed, never executed.)
local f = io.open("nul", "r")
f:seek("end")
f:setvbuf("full", 4096)

return f

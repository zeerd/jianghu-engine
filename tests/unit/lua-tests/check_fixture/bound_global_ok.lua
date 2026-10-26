-- check-lua fixture: globals the engine does bind must not be flagged --
-- guards that the whitelist is read from the live _G, not a stale list.
return function(me)
    notify_fail(me, "x")
    log_info("hello %d", 1)
end

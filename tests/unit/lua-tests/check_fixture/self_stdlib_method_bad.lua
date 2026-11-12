-- check-lua fixture: the stdlib ':' whitelist is name-based (math.random,
-- string.format), but self is the object userdata — it carries none of those
-- fields, so the call fails at runtime like any other unbound method.
return function(self)
    return self:random(3)
end

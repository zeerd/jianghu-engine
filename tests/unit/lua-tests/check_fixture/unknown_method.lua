-- check-lua fixture: ':' resolves against the object metatable, so the
-- method must be bound there -- this one is not.
return function(self)
    self:no_such_method_xyz()
end

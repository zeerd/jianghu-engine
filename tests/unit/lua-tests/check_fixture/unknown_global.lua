-- check-lua fixture: the engine never bound this global, so reading it is
-- "attempt to call a nil value (global ...)" at runtime.
return function(me)
    definitely_not_bound_xyz(me)
end

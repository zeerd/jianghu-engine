-- Fixture: a global assigned inside a function nested in a table constructor
-- is a statement-level definition, not a table field -- brace depth alone used
-- to classify it as a field, so the definition was lost.
return { init = function() shared_in_table_fn = 1 end }

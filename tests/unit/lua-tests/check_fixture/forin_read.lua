-- Fixture: reads of globals defined after/inside a `for ... in` loop must
-- resolve against the definitions collected from the other checked files.
return forin_helper(1), forin_flag, in_loop_total

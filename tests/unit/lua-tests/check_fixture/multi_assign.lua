-- Fixture: `a, b = ...` defines BOTH targets and neither of them is a read.
-- Regression: only the name directly in front of `=` counted as an assignment
-- target, so `term` was reported as an unbound global twice (as a read on the
-- assignment line and again on the return line).
term, kind = 1, 2

return term, kind

-- check-lua fixture: tables that are 0-based BY DESIGN must not be flagged.
-- 证据1：`for i = 0, n - 1` 建表；证据2：字面量 `T[0]`；证据3：`{ [0] = ... }`。
local counts = {}
for i = 0, 4 do
    counts[i] = 0
end
local roll = random(5)
counts[roll] = counts[roll] + 1

local grid = {}
grid[0] = "start"
grid[1] = "next"
local step = random(2)

local weighted = {
    [0] = "a",
    [1] = "b",
}
local pick = random(2)

return { counts = counts, at = grid[step], pick = weighted[pick] }

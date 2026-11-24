-- test_level_exp.lua — 升级经验表 lua-default/data/level_exp.lua 的数据契约
--
-- 表由 CChar::level2_exp 经 require("data.level_exp") 读取
-- （src/char/CharLevel.cpp），本文件守护其形状契约：
-- 首级 0 经验、键 1..175 连续、经验严格递增。

local lu = require("luaunit")

TestLevelExp = {}

local function load()
    return require("data.level_exp")
end

function TestLevelExp:test_first_level_needs_zero_exp()
    lu.assertEquals(load()[1], 0)
end

function TestLevelExp:test_keys_cover_1_to_175_contiguously()
    local t = load()
    for lv = 1, 175 do
        lu.assertTrue(type(t[lv]) == "number",
            "level " .. lv .. " missing or not a number")
    end
end

function TestLevelExp:test_exp_is_strictly_increasing()
    local t = load()
    for lv = 2, 175 do
        lu.assertTrue(t[lv] > t[lv - 1],
            "level " .. lv .. " exp " .. tostring(t[lv])
            .. " <= previous " .. tostring(t[lv - 1]))
    end
end

return TestLevelExp

-- lua_unit_runner.lua — Lua 脚本单测执行器（test_lua_unit.cpp 桥的 Lua 侧）
--
-- 职责：
--   1. 扩展 package.path 指向 3rd/lua submodule 的 luaunit；
--   2. 逐文件装载 test_*.lua（经引擎 GBK require），执行其 Test* 用例；
--   3. 汇总报告（执行总数 / 失败明细 / 装载错误，均为格式化字符串）
--      回传 C++，由桥转成 gtest 断言。
--
-- 覆盖率不归本模块：采集窗口横跨整个测试进程，见 lib/lua_coverage.lua。
--
-- 测试文件契约：定义全局 Test* 类（表，方法以 test 开头）或 test* 全局
-- 函数；执行完该文件后其新增的 Test* 全局被清除，文件之间互不残留；
-- 辅助函数不得以 test 开头（会被当作用例执行）。

local M = {}

-- 3rd/lua submodule 经仓库相对路径解析（测试固定以仓库根为 CWD）
if not package.path:find("3rd/lua/luaunit", 1, true) then
    package.path = package.path .. ";3rd/lua/luaunit/?.lua"
end

local lu = require("luaunit")

local function is_test_name(name)
    return type(name) == "string" and name:sub(1, 4):lower() == "test"
end

-- 当前 _G 中的 Test* 名集合（差分出单个文件新增的用例）
local function test_globals()
    local set = {}
    for k in pairs(_G) do
        if is_test_name(k) then set[k] = true end
    end
    return set
end

-- 执行单个测试模块，把失败/错误以格式化字符串追加进 report
local function run_module(mod, report)
    local before = test_globals()
    package.loaded[mod] = nil

    local ok, err = pcall(require, mod)
    if ok then
        -- 该文件新增的 Test* → {名字, 实例} 列表（LuaUnit 的输入格式）
        local list = {}
        for name, value in pairs(_G) do
            if not before[name] and is_test_name(name)
                and (type(value) == "table" or type(value) == "function") then
                list[#list + 1] = { name, value }
            end
        end
        table.sort(list, function(a, b) return a[1] < b[1] end)

        if #list == 0 then
            report.errors[#report.errors + 1] =
                string.format("%s: 未定义任何 Test* 用例", mod)
        else
            local runner = lu.LuaUnit.new()
            runner:runSuiteByInstancesNoCmdLineParsing(list)

            local result = runner.result
            report.total = report.total + (result.runCount or 0)
            for _, node in ipairs(result.allTests) do
                if node:isFailure() or node:isError() then
                    report.failures[#report.failures + 1] = string.format(
                        "%s :: %s\n%s\n%s", mod, node.testName or "?",
                        node.msg or "", node.stackTrace or "")
                end
            end
        end
    else
        report.errors[#report.errors + 1] =
            string.format("%s: 装载失败: %s", mod, tostring(err))
    end

    -- 清理：该文件新增的 Test* 全局 + 自身的 require 缓存
    for k in pairs(_G) do
        if not before[k] and is_test_name(k) then _G[k] = nil end
    end
    package.loaded[mod] = nil
end

-- 入口：modules = 模块名数组。
-- 返回 { total, failures = {字符串...}, errors = {...} }
function M.run(modules)
    local report = { total = 0, failures = {}, errors = {} }
    for i = 1, #modules do
        run_module(modules[i], report)
    end
    return report
end

return M

-- lua_coverage.lua — Lua 语料覆盖率的采集窗口（test_lua_coverage.cpp 的 Lua 侧）
--
-- 窗口横跨整个测试进程，而非只在 Lua 单测期间开窗：语料的首次执行常落在
-- Lua 单测之外（C++ 测试经 load_script / require 加载同一批 lua-default
-- 脚本），且共享 lua_State 的 package.loaded 与脚本缓存都跨用例存活——
-- 晚开窗记录到的只是「已无人执行」的空档。
--
-- 职责：
--   1. start(statsfile)：开窗（luacov init）。幂等；采集本身出错不抛，
--      采集缺席不该让被测的测试变红。
--   2. finish(corpus, reportfile)：收窗存档，再按 luacov 报告同口径
--      （LineScanner 判行是否计入分母）产出行级明细，供上层跨片合并。
--
-- 明细是行级而非每文件计数——计数在多采集面之间不可合并（分母会随份数
-- 翻倍），行号集合才能取并集：
--   N,<文件>,<恒计行数>   无论命中与否都进分母的行数。只由文件内容决定，
--                         各采集面必然一致，取一即可
--   H,<文件>,<行号>       该行被命中过（进分子）
--   C,<文件>,<行号>       该行被命中过、且属「未命中才排除」的行
--                         （分子分母都算它）
--
-- 分子分母在合并侧按下式合成，任一采集面都给不出完整答案——luacov 对
-- 「未命中才排除」的行只在真被命中时才计入分母，故分母本身随命中变化：
--   分子 = |H|，分母 = N + |C|
-- 恒有 |H| = |H∩恒计| + |C| ≤ N + |C|，覆盖率因此恒 ≤ 100%。

local M = {}

-- luacov 经仓库相对路径解析（测试固定以仓库根或分片根为 CWD）。
-- 本模块先于引擎绑定被装载，package.path 里还没有 luacov 的路径。
if not package.path:find("3rd/lua/luacov", 1, true) then
    package.path = package.path .. ";3rd/lua/luacov/src/?.lua"
end

local cov
-- 窗口开启前的原始实现：luacov init 会 patch os.exit 与
-- coroutine.create/wrap（per-thread hook 所需），收窗时一并还原，
-- 不把采集状态泄漏给同一 lua_State 里的其他测试。
local raw = { os.exit, coroutine.create, coroutine.wrap }

local function fail(...)
    io.stderr:write("[lua-coverage] ", ..., "\n")
end

function M.start(statsfile)
    if cov then return end
    if not statsfile then
        fail("start: no statsfile")
        return
    end
    local ok, runner = pcall(require, "luacov.runner")
    if not ok then
        fail("start: require luacov.runner failed: ", tostring(runner))
        return
    end
    cov = runner
    cov.init({
        statsfile = statsfile,
        -- 绝对路径占位：set_config 对相对 reportfile 会走 io.popen 取 CWD；
        -- runreport=false 故该文件不会被创建，只为跳过 popen
        reportfile = statsfile .. ".report",
        codefromstrings = true,  -- 引擎 luaL_loadbuffer 的 chunkname 不带 @
        -- 分母按交付取：lua-default（默认数据集）与 lua-sample（样例世界）
        -- 都随引擎交付，都在分母里。tests/unit/lua-tests 是测试夹具，不入分母。
        -- sample 侧的用例跑在独立配置组（script_root = lua-sample），
        -- 样本由引擎作为根直接加载，命中因此归得到 sample 名下。
        include = { "lua%-default/", "lua%-sample/" },
    })
end

-- 从 statsfile 回读而非用内存数据：shutdown 走 save_stats 后
-- runner.data 已被清空，回读的正是落盘产物本身。
local function write_report(statsfile, corpus, reportfile)
    local probe = io.open(statsfile, "r")
    if not probe then
        fail("report: cannot read statsfile ", statsfile)
        return
    end
    probe:close()

    local ok_stats, stats_mod = pcall(require, "luacov.stats")
    local ok_scan, LineScanner = pcall(require, "luacov.linescanner")
    if not (ok_stats and ok_scan) then
        fail("report: require failed: ", tostring(stats_mod), " / ",
             tostring(LineScanner))
        return
    end

    local data = stats_mod.load(statsfile) or {}
    local out, err = io.open(reportfile, "w")
    if not out then
        fail("report: cannot write ", reportfile, ": ", tostring(err))
        return
    end

    for i = 1, #corpus do
        local file = corpus[i]
        local fd = io.open(file, "r")
        if fd then
            local filedata = data[file] or {}
            local scanner = LineScanner:new()
            local line_nr, unconditional = 0, 0
            while true do
                local line = fd:read("*l")
                if not line then break end
                line_nr = line_nr + 1
                local always_excl, excl_when_miss = scanner:consume(line)
                -- 与 luacov reporter 同式：恒排除的行不进任何一侧
                if not always_excl then
                    local hits = filedata[line_nr] or 0
                    if not excl_when_miss then
                        unconditional = unconditional + 1
                    elseif hits > 0 then
                        out:write("C,", file, ",", line_nr, "\n")
                    end
                    if hits > 0 then
                        out:write("H,", file, ",", line_nr, "\n")
                    end
                end
            end
            fd:close()
            out:write("N,", file, ",", unconditional, "\n")
        end
    end
    out:close()
end

function M.finish(corpus, reportfile)
    if not cov then
        fail("finish: coverage window was never opened")
        return
    end
    local statsfile = cov.configuration and cov.configuration.statsfile
    cov.shutdown()
    os.exit = raw[1]
    coroutine.create = raw[2]
    coroutine.wrap = raw[3]
    cov.initialized = false
    cov = nil
    if statsfile and reportfile then
        write_report(statsfile, corpus, reportfile)
    else
        fail("finish: missing statsfile/reportfile")
    end
end

return M

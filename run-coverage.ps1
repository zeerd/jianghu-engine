# run-coverage.ps1 - Collect code coverage for tests
#
# Usage:
#   powershell -File run-coverage.ps1                    # line coverage (default)
#   powershell -File run-coverage.ps1 -CoverageType branch  # branch coverage
#   powershell -File run-coverage.ps1 -ListNotInstrumented  # also list files absent from the report
#   powershell -File run-coverage.ps1 -CovKeepCount 3       # 只保留最近 3 份 .cov（0 = 不清理）
#   powershell -File run-coverage.ps1 -SummaryKeepCount 5   # summary.csv 与 summary-*.txt 各留最近 5 份（0 = 不清理）
#   powershell -File run-coverage.ps1 -TestTmpDir D:\jt     # 测试临时目录（默认 <仓库根>\test_tmp）
#   powershell -File run-coverage.ps1 -Shards 4             # 引擎测试分片数（0 = 自动）
#
# Requires: OpenCppCoverage (https://github.com/OpenCppCoverage/OpenCppCoverage)
#           installed and on PATH, or set $OpenCppCoveragePath below.
#
# What it does:
#   1. Builds tests (Release|Win32 — 生产同构配置；Debug 构建的
#      测试程序内含 TestConfigGate，运行即报错，故默认禁止)。
#      引擎测试（tests）与 CLI 测试（cli\tests）分别构建。
#   2. Discovers test suites (--gtest_list_tests), packs them into
#      -Shards deterministic groups, gives each shard a private working
#      directory (per-shard fixture tree + JIANGHU_TEST_TMP), then runs
#      all engine shards + the CLI lane in parallel under OpenCppCoverage,
#      each exporting a binary .cov and a gtest XML.
#      Aggregates results per shard (exit code / gtest failures / test-count
#      cross-check against discovery).
#   3. Merges every shard .cov into one report with a final pass
#      (multiple --input_coverage) → HTML in coverage_report/ + Cobertura.
#   4. Saves binary .cov to coverage_history/ (pruned to newest -CovKeepCount)
#   5. Appends summary to coverage_history/summary.csv
#      (pruned to newest -SummaryKeepCount rows; per-run summary txt likewise)
#   6. Shows coverage delta vs previous run (同配置基线比较)
#   7. Saves the Coverage Summary text to
#      coverage_history/summary-<timestamp>.txt
#      （csv 行与该类 txt 均只保留最近 -SummaryKeepCount 份）
#
# 配置说明：覆盖率与验收均在 Release 上采集。历史运行全为 Debug，
# 切换后首条记录 delta 显示 n/a 并成为新的 Release 基线。

param(
    [string]$OpenCppCoveragePath = "",
    [string]$ServerSrc = "src",
    [string]$CliSrc = "cli\src",
    # 唯一被认可的测试配置 = 生产配置 Release|Win32（/MT, NDEBUG, /O2）。
    # Debug 构建的 tests.exe 含 TestConfigGate：无论以何种方式运行都会
    # 在测试结果中无条件报错（见 tests/test_env.cpp），杜绝拿 Debug
    # 假绿当验收。实例：SYMBOL_INFO 栈溢出 Debug 1216 全绿、Release 98 例崩。
    [string]$BuildConfig = "Release",
    [string]$BuildPlatform = "Win32",
    [ValidateSet("line", "branch")]
    [string]$CoverageType = "line",
    [switch]$ListNotInstrumented,
    # .cov 只是 OpenCppCoverage 的二进制导出（事后合并/重放报告用），
    # delta 比较读的是 summary.csv，与 .cov 无关；因此只保留最近 N 份。
    # 0 = 不清理（保留全部）。
    [int]$CovKeepCount = 5,
    # summary 历史（csv 行 + 每轮一份的 summary-<时间戳>.txt）只保留最近 N 条/份。
    # csv 是给人看的趋势记录，delta 只依赖同配置最近一条，条数下限不受影响；
    # 若某种配置连续被 N 轮其他配置隔开，其基线被清理后下一轮显示 n/a 并重建基线。
    # 0 = 不清理（保留全部）。
    [int]$SummaryKeepCount = 5,
    # 测试临时目录（单测/CLI 测试经 JIANGHU_TEST_TMP 消费，见 tests\test_tmp_dir.h）。
    # 路径由本脚本决定、测试代码只读不写死；0/空 = <仓库根>\test_tmp。
    [string]$TestTmpDir = "",
    # 引擎测试分片数：0 = 自动（逻辑核数，上限 8、下限 1，且不超过套件数）。
    # CLI 测试（274 例）作为独立一路与引擎分片同时跑，不参与分片数。
    [int]$Shards = 0
)

$ErrorActionPreference = "Stop"
$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path
# 输出目录：Debug 工程显式设为 tests\Debug\；Release 用 MSBuild 默认
# $(SolutionDir)Release\（仓库根的 Release\）
$TestExe = if ($BuildConfig -eq "Debug") {
    Join-Path $RootDir "tests\Debug\tests.exe"
} else {
    Join-Path $RootDir "Release\tests.exe"
}
# CLI 测试工程独立（cli\tests\jianghu_cli_tests.vcxproj），产物固定输出到
# bin\cli\；由本脚本分别构建与运行，覆盖率合并后统一展示。
$CliTestExe = Join-Path $RootDir "bin\cli\jianghu_cli_tests.exe"
$CoverageDir = Join-Path $RootDir "coverage_report"
$HistoryDir = Join-Path $RootDir "coverage_history"
$SummaryCsv = Join-Path $HistoryDir "summary.csv"
$Today = Get-Date -Format "yyyy-MM-dd_HHmmss"
$CovBinary = Join-Path $HistoryDir "$Today.cov"
# Coverage Summary 段的控制台输出同时落盘为本次运行的文本记录
$SummaryLog = Join-Path $HistoryDir "summary-$Today.txt"
$SummaryLogLines = New-Object 'System.Collections.Generic.List[string]'
function Write-SummaryLine {
    param([string]$Text = "", $Color = "Gray")
    Write-Host $Text -ForegroundColor $Color
    $script:SummaryLogLines.Add($Text)
}

# --- Step 0: Find OpenCppCoverage ---
if (-not $OpenCppCoveragePath) {
    # Check PATH first
    $found = Get-Command OpenCppCoverage -ErrorAction SilentlyContinue
    if ($found) {
        $OpenCppCoveragePath = $found.Source
    } else {
        # Check default install location
        $defaultPath = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"
        if (Test-Path $defaultPath) {
            $OpenCppCoveragePath = $defaultPath
        }
    }
}

# --- Step 0: Check OpenCppCoverage ---
if (-not $OpenCppCoveragePath -or -not (Test-Path $OpenCppCoveragePath)) {
    Write-Host "ERROR: OpenCppCoverage not found at '$OpenCppCoveragePath'" -ForegroundColor Red
    Write-Host "Download from: https://github.com/OpenCppCoverage/OpenCppCoverage/releases" -ForegroundColor Yellow
    Write-Host "Or set -OpenCppCoveragePath to the full path." -ForegroundColor Yellow
    exit 1
}
Write-Host "Using OpenCppCoverage: $OpenCppCoveragePath" -ForegroundColor DarkGray
Write-Host "Coverage type: $CoverageType" -ForegroundColor DarkGray

# --- Step 1: Build ---
Write-Host "=== Building tests ($BuildConfig|$BuildPlatform) ===" -ForegroundColor Cyan

# Find MSBuild
$msbuildExe = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
if (-not $msbuildExe) {
    $msbuildExe = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
}
if (-not (Test-Path $msbuildExe)) {
    Write-Host "ERROR: MSBuild not found!" -ForegroundColor Red
    exit 1
}
Write-Host "Using MSBuild: $msbuildExe" -ForegroundColor DarkGray

# 引擎测试与 CLI 测试分别构建，共享同一 Release|Win32 配置。
# 工程路径拼成绝对路径：msbuild 按当前目录解析相对路径，脚本不应依赖调用方 CWD。
$testProjects = @(
    (Join-Path $RootDir "tests\unit\tests.vcxproj"),
    (Join-Path $RootDir "cli\tests\jianghu_cli_tests.vcxproj")
)
foreach ($proj in $testProjects) {
    $msbuildArgs = @(
        $proj,
        "/p:Configuration=$BuildConfig",
        "/p:Platform=$BuildPlatform",
        "/nologo", "/verbosity:minimal"
    )
    & $msbuildExe $msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Build failed ($proj)!" -ForegroundColor Red
        exit 1
    }
}
Write-Host "Build succeeded." -ForegroundColor Green

# --- Step 2: Ensure directories ---
if (-not (Test-Path $HistoryDir)) { New-Item -ItemType Directory -Path $HistoryDir | Out-Null }

# 测试临时目录：路径由本脚本决定，经 JIANGHU_TEST_TMP 注入给两个测试工程
# （消费点见 tests\test_tmp_dir.h；未注入时测试回落 <CWD>\tmp，仅供直跑）。
# 必须规范化为绝对路径：引擎分片的 CWD 是各自分片目录，相对路径会各落各家。
if (-not $TestTmpDir) { $TestTmpDir = Join-Path $RootDir "test_tmp" }
$TestTmpDir = [System.IO.Path]::GetFullPath($TestTmpDir)
New-Item -ItemType Directory -Force -Path $TestTmpDir | Out-Null
$env:JIANGHU_TEST_TMP = $TestTmpDir
Write-Host "Test temp dir: $TestTmpDir" -ForegroundColor DarkGray

# --- Step 2.5: 套件发现与分片（确定性装箱） ---
Write-Host "`n=== Discovering test suites ===" -ForegroundColor Cyan

# --gtest_list_tests 只列不跑；CWD 钉在仓库根（配置按相对路径读取）。
# 解析格式：套件行 = `Name.`（行首无缩进），用例行 = 缩进 + 首 token；
# DISABLED_ 用例默认不执行，不计入发现数（与 gtest XML 的 tests 口径对齐）。
function Get-GtestSuites {
    param([string]$ExePath)
    Push-Location $RootDir
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $lines = & $ExePath --gtest_list_tests 2>$null
        $listExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prevEAP
        Pop-Location
    }
    if ($listExit -ne 0) {
        Write-Host "ERROR: --gtest_list_tests failed for $ExePath (exit $listExit)!" -ForegroundColor Red
        exit 1
    }
    $suites = [ordered]@{}
    $current = $null
    foreach ($line in $lines) {
        if ($line -match '^(\S+)\.\s*$') {
            $current = $Matches[1]
            if (-not $suites.Contains($current)) { $suites[$current] = 0 }
        }
        elseif ($null -ne $current -and $line -match '^\s+\S') {
            $first = ($line.Trim() -split '\s+')[0]
            if ($first -notlike 'DISABLED_*') { $suites[$current] = [int]$suites[$current] + 1 }
        }
    }
    return , $suites
}

$engineSuites = Get-GtestSuites $TestExe
$cliSuites    = Get-GtestSuites $CliTestExe
$engineTotal = 0
foreach ($v in $engineSuites.Values) { $engineTotal += [int]$v }
$cliTotal = 0
foreach ($v in $cliSuites.Values) { $cliTotal += [int]$v }
if ($engineSuites.Count -eq 0 -or $engineTotal -eq 0) {
    Write-Host "ERROR: no test suites discovered in $TestExe!" -ForegroundColor Red
    exit 1
}
# 套件名必须能安全进 --gtest_filter（空格/`-`/`:`/`*`/`?` 会破坏 filter 语义）
foreach ($suiteName in $engineSuites.Keys) {
    if ($suiteName -notmatch '^[A-Za-z0-9_/.]+$') {
        Write-Host "ERROR: suite name '$suiteName' contains chars unsafe for --gtest_filter!" -ForegroundColor Red
        exit 1
    }
}

# 分片数：0 = 自动（逻辑核数、上限 8），再夹紧到 [1, 套件数]
$shardCount = $Shards
$shardSource = "manual"
if ($shardCount -le 0) {
    $shardCount = [Math]::Min([Environment]::ProcessorCount, 8)
    $shardSource = "auto"
}
if ($shardCount -gt $engineSuites.Count) { $shardCount = $engineSuites.Count }
if ($shardCount -lt 1) { $shardCount = 1 }

# 贪心装箱（LPT）：按用例数降序、并列按名序 → 每套件进当前最轻的分片
# （并列取序号小者）。排序键唯一（套件名不重复）→ 装箱结果与机器无关、可复现。
$entries = foreach ($kv in $engineSuites.GetEnumerator()) {
    [pscustomobject]@{ Name = [string]$kv.Key; Count = [int]$kv.Value }
}
$sortedEntries = $entries | Sort-Object -Property @{Expression = "Count"; Descending = $true}, @{Expression = "Name"}
$bucketNames = @()
$bucketCounts = @()
for ($i = 0; $i -lt $shardCount; $i++) { $bucketNames += , @(); $bucketCounts += 0 }
foreach ($e in $sortedEntries) {
    $minIdx = 0
    for ($i = 1; $i -lt $shardCount; $i++) {
        if ($bucketCounts[$i] -lt $bucketCounts[$minIdx]) { $minIdx = $i }
    }
    $bucketNames[$minIdx] += $e.Name
    $bucketCounts[$minIdx] += $e.Count
}
# 硬校验：装箱无遗漏、无空分片（filter 为空会退化成“跑全部”，必须拦死）
$packedTotal = 0
foreach ($c in $bucketCounts) { $packedTotal += $c }
if ($packedTotal -ne $engineTotal) {
    Write-Host "ERROR: partition lost tests: packed $packedTotal != discovered $engineTotal!" -ForegroundColor Red
    exit 1
}
$filters = @()
for ($i = 0; $i -lt $shardCount; $i++) {
    $members = @($bucketNames[$i])
    if ($members.Count -eq 0) {
        Write-Host "ERROR: shard s$i is empty (filter would run ALL tests)!" -ForegroundColor Red
        exit 1
    }
    $filters += (($members | ForEach-Object { "$_.*" }) -join ':')
}

Write-Host ("engine: {0} suites / {1} tests -> {2} shard(s) [{3}]; cli: {4} suites / {5} tests -> 1 lane" -f `
        $engineSuites.Count, $engineTotal, $shardCount, $shardSource, $cliSuites.Count, $cliTotal) -ForegroundColor DarkGray
for ($i = 0; $i -lt $shardCount; $i++) {
    Write-Host ("  s{0}: {1} suites / {2} tests" -f $i, @($bucketNames[$i]).Count, $bucketCounts[$i]) -ForegroundColor DarkGray
}

# --- Step 2.6: 分片工作目录（隔离靠构造，不靠「哪些套件有冲突」的名单） ---
# 测试对仓库的一切相对路径读写（test_config.ini、lua-tests 夹具改写、
# lua-default/lua-sample、map.txt、日志）都相对 CWD 解析；tests.exe 还依赖
# CWD 命中仓库根的 libmySQL.dll（Release\ 无该 DLL，换 CWD 即 0xC0000135）。
# 因此每分片拷出一棵完整私有树 + DLL，跨进程互踩在结构上不存在；
# 每轮重建 → 夹具必然新鲜（上轮崩溃残留不会污染本轮）。
Write-Host "`n=== Preparing shard workspaces ===" -ForegroundColor Cyan
$ShardsRoot = Join-Path $TestTmpDir "shards"
if (Test-Path -LiteralPath $ShardsRoot) {
    Remove-Item -LiteralPath $ShardsRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $ShardsRoot | Out-Null
for ($i = 0; $i -lt $shardCount; $i++) {
    $wd = Join-Path $ShardsRoot "s$i"
    New-Item -ItemType Directory -Force -Path (Join-Path $wd "tests\unit\log") | Out-Null
    Copy-Item (Join-Path $RootDir "tests\unit\*.ini") (Join-Path $wd "tests\unit") -Force
    Copy-Item (Join-Path $RootDir "tests\unit\lua-tests") (Join-Path $wd "tests\unit") -Recurse -Force
    Copy-Item (Join-Path $RootDir "lua-default") $wd -Recurse -Force
    Copy-Item (Join-Path $RootDir "lua-sample") $wd -Recurse -Force
    Copy-Item (Join-Path $RootDir "libmySQL.dll") $wd -Force
}
Write-Host "Shard workspaces: $ShardsRoot (rebuilt this run)" -ForegroundColor DarkGray

# --- Step 3: 并行执行（N 个引擎分片 + 1 路 CLI，各自独立 OCC 实例） ---
Write-Host "`n=== Running tests with coverage ===" -ForegroundColor Cyan

$sourcePath = Join-Path $RootDir $ServerSrc
$cliSourcePath = Join-Path $RootDir $CliSrc

# 分支覆盖率开关（仅在请求时添加；与串行版一致——每个 instrumenting 的
# OCC 实例与末尾合并通行证都带上）
$branchArg = @()
if ($CoverageType -eq "branch") {
    $branchArg = @("--branch_coverage")
}

# 给子进程参数逐项加引号（ProcessStartInfo.Arguments 只收单个字符串）。
function Quote-Arg([string]$s) {
    if ($s -match '[\s"]') { return '"' + ($s -replace '"', '\"') + '"' }
    return $s
}

# 无窗口启动一路测试（OCC → 测试进程），stdout/stderr 异步排空到内存
# （不读管道会堵死子进程），退出后由调用方落盘到分片目录。
function Start-LaneProcess {
    param([string]$Exe, [string[]]$Arguments, [string]$WorkDir)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $psi.WorkingDirectory = $WorkDir
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    return [pscustomobject]@{
        Process = $p
        StdOut  = $p.StandardOutput.ReadToEndAsync()
        StdErr  = $p.StandardError.ReadToEndAsync()
    }
}

$lanes = @()
try {
    for ($i = 0; $i -lt $shardCount; $i++) {
        $wd = Join-Path $ShardsRoot "s$i"
        $xmlPath = Join-Path $wd "s$i.xml"
        $covPath = Join-Path $wd "s$i.cov"
        $laneArgs = $branchArg + @(
            "--sources", "$sourcePath\*",
            "--modules", $TestExe,
            "--working_dir", $wd,
            "--export_type", "binary:$covPath",
            "--"
        ) + @($TestExe, "--gtest_filter=$($filters[$i])", "--gtest_output=xml:$xmlPath")
        # 本分片独立的测试临时目录：固定名文件（jh_baseline_* 等）跨片不相撞
        $env:JIANGHU_TEST_TMP = Join-Path $wd "tmp"
        $run = Start-LaneProcess -Exe $OpenCppCoveragePath -Arguments $laneArgs -WorkDir $wd
        $lanes += [pscustomobject]@{
            Name     = "s$i"
            Run      = $run
            Expected = $bucketCounts[$i]
            Xml      = $xmlPath
            Cov      = $covPath
            OutLog   = Join-Path $wd "s$i.out.log"
            ErrLog   = Join-Path $wd "s$i.err.log"
            Started  = Get-Date
            ExitCode = $null
            Elapsed  = $null
        }
        Write-Host ("  -> s{0}: {1} suites / {2} tests" -f $i, @($bucketNames[$i]).Count, $bucketCounts[$i]) -ForegroundColor DarkGray
    }
    # CLI 路：工作目录与测试临时目录保持串行版原样（CLI 测试全部经
    # JIANGHU_TEST_TMP 写临时目录、无仓库相对路径依赖）。
    $env:JIANGHU_TEST_TMP = $TestTmpDir
    $cliXml = Join-Path $ShardsRoot "cli.xml"
    $cliCov = Join-Path $ShardsRoot "cli.cov"
    $cliArgs = $branchArg + @(
        "--sources", "$sourcePath\*",
        "--sources", "$cliSourcePath\*",
        "--modules", $CliTestExe,
        "--working_dir", $RootDir,
        "--export_type", "binary:$cliCov",
        "--"
    ) + @($CliTestExe, "--gtest_output=xml:$cliXml")
    $cliRun = Start-LaneProcess -Exe $OpenCppCoveragePath -Arguments $cliArgs -WorkDir $RootDir
    $lanes += [pscustomobject]@{
        Name     = "cli"
        Run      = $cliRun
        Expected = $cliTotal
        Xml      = $cliXml
        Cov      = $cliCov
        OutLog   = Join-Path $ShardsRoot "cli.out.log"
        ErrLog   = Join-Path $ShardsRoot "cli.err.log"
        Started  = Get-Date
        ExitCode = $null
        Elapsed  = $null
    }
    Write-Host ("  -> cli: {0} suites / {1} tests" -f $cliSuites.Count, $cliTotal) -ForegroundColor DarkGray
}
catch {
    # 启动失败：终止本轮已启动的自有进程（按对象/句柄，不按进程名），不留孤儿
    foreach ($l in $lanes) {
        if ($l.Run -and -not $l.Run.Process.HasExited) {
            try { $l.Run.Process.Kill() } catch { }
        }
    }
    $env:JIANGHU_TEST_TMP = $TestTmpDir
    Write-Host "ERROR: failed to start test lanes: $_" -ForegroundColor Red
    exit 1
}
$env:JIANGHU_TEST_TMP = $TestTmpDir

# 等待全部分片退出；每片退出即落盘输出并报告（失败片当场变红）
$totalStart = Get-Date
$pending = @($lanes)
while ($pending.Count -gt 0) {
    Start-Sleep -Milliseconds 300
    $still = @()
    foreach ($l in $pending) {
        if ($l.Run.Process.HasExited) {
            $proc = $l.Run.Process
            $proc.WaitForExit()
            $l.ExitCode = $proc.ExitCode
            $l.Elapsed = (Get-Date) - $l.Started
            try {
                Set-Content -LiteralPath $l.OutLog -Value $l.Run.StdOut.Result -Encoding UTF8
                Set-Content -LiteralPath $l.ErrLog -Value $l.Run.StdErr.Result -Encoding UTF8
            } catch { }
            $laneColor = if ($l.ExitCode -eq 0) { "Gray" } else { "Red" }
            Write-Host ("  <- {0}: exit={1} ({2:N1}s)" -f $l.Name, $l.ExitCode, $l.Elapsed.TotalSeconds) -ForegroundColor $laneColor
        }
        else { $still += $l }
    }
    $pending = $still
}
$wallSec = ((Get-Date) - $totalStart).TotalSeconds

# --- Step 3.1: 结果聚合（gtest XML 为事实源 + 三重硬校验） ---
# 每片独立判定：退出码、XML 失败数、执行数 == 发现数（filter 拼错/漏套件/
# 进程崩溃都会在这里被拦下）；任一片不过即整轮失败、不进合并。
Write-Host "`n=== Test results ===" -ForegroundColor Cyan
$allIssues = @()
$failedNames = @()
$engineRan = 0
$cliRan = 0
foreach ($l in $lanes) {
    $doc = $null
    if (Test-Path -LiteralPath $l.Xml) {
        try { $doc = [xml](Get-Content -LiteralPath $l.Xml -Raw -Encoding UTF8) } catch { $doc = $null }
    }
    $tests = -1
    $failed = 0
    if ($null -ne $doc -and $null -ne $doc.testsuites) {
        $tests = [int]$doc.testsuites.tests
        $failed = [int]$doc.testsuites.failures
        foreach ($ts in @($doc.testsuites.testsuite)) {
            foreach ($tc in @($ts.testcase)) {
                if ($tc.failure) { $failedNames += "$($ts.name).$($tc.name)" }
            }
        }
    }
    $issues = @()
    if ($l.ExitCode -ne 0) { $issues += "exit $($l.ExitCode)" }
    if ($null -eq $doc) { $issues += "gtest XML 缺失/不可解析（疑似崩溃）" }
    else {
        if ($null -eq $doc.testsuites) { $issues += "gtest XML 结构异常" }
        else {
            if ($failed -gt 0) { $issues += "$failed failed" }
            if ($tests -ne $l.Expected) { $issues += "执行 $tests != 发现 $($l.Expected)（filter 分片有误）" }
        }
    }
    $status = if ($issues.Count -eq 0) { "OK  " } else { "FAIL" }
    $rowColor = if ($issues.Count -eq 0) { "Green" } else { "Red" }
    $elapsedSec = if ($l.Elapsed) { $l.Elapsed.TotalSeconds } else { 0.0 }
    Write-Host ("  [{0}] {1,-4} tests={2,-5} expected={3,-5} exit={4,-4} {5,6:N1}s" -f `
            $status, $l.Name, $tests, $l.Expected, $l.ExitCode, $elapsedSec) -ForegroundColor $rowColor
    if ($issues.Count -gt 0) {
        $allIssues += ("{0}: {1}  [log: {2}]" -f $l.Name, ($issues -join ', '), $l.OutLog)
    }
    if ($l.Name -eq "cli") { $cliRan += [Math]::Max($tests, 0) }
    else { $engineRan += [Math]::Max($tests, 0) }
}
if ($engineRan -ne $engineTotal) {
    $allIssues += ("engine 汇总执行 {0} != 发现总数 {1}" -f $engineRan, $engineTotal)
}
if ($cliRan -ne $cliTotal) {
    $allIssues += ("cli 汇总执行 {0} != 发现总数 {1}" -f $cliRan, $cliTotal)
}
Write-Host ("  engine: {0}/{1} executed; cli: {2}/{3} executed; wall {4:N1}s" -f `
        $engineRan, $engineTotal, $cliRan, $cliTotal, $wallSec) -ForegroundColor DarkGray
if ($failedNames.Count -gt 0) {
    Write-Host "`nFailed tests:" -ForegroundColor Red
    foreach ($n in ($failedNames | Select-Object -First 100)) { Write-Host "  $n" -ForegroundColor Red }
    if ($failedNames.Count -gt 100) { Write-Host ("  ... and {0} more" -f ($failedNames.Count - 100)) -ForegroundColor Red }
}
if ($allIssues.Count -gt 0) {
    Write-Host "`nERROR: tests failed:" -ForegroundColor Red
    foreach ($issue in $allIssues) { Write-Host "  $issue" -ForegroundColor Red }
    Write-Host "Shard workspaces kept for diagnosis: $ShardsRoot" -ForegroundColor Yellow
    exit 1
}

# --- Step 3.2: 覆盖率合并通行证（多路 --input_coverage 并集 → 最终报告） ---
# 与 run-integration-tests.ps1 的合并通行证同款：child 用立即退出的 cmd
# （--sources 只圈 src/cli\src，child 不贡献覆盖）。OCC 0.9.x 重建该 child
# 命令行时 cmd 会报 `not recognized` 并以 1 退出（集测同款既有行为，见
# coverage_history\e2e-sample-opencppcoverage-merge.log）——输入覆盖已加载，
# 产物不受影响，故**退出码不作判据、以产物为准**：先删旧 coverage.xml，
# 合并后它必须重新出现且行数非零——这是 delta 历史不被空报告污染的闸门。
# 输出捕获落盘（不直喷控制台：stderr 会变错误记录，2>&1 调用方 $? 为 false）。
Write-Host "`n=== Merging coverage ($($lanes.Count) inputs) ===" -ForegroundColor Cyan
$coberturaOut = Join-Path $CoverageDir "coverage.xml"
Remove-Item -LiteralPath $coberturaOut -Force -ErrorAction SilentlyContinue
# 参数一律传纯值、不手工包引号：PowerShell 调用原生程序按需自动加引号；
# 手工写入的 `"` 在 pwsh 7（Standard 传递）下会作为字面量送达子进程。
$mergeArgs = $branchArg + @(
    "--sources", "$sourcePath\*",
    "--sources", "$cliSourcePath\*",
    "--working_dir", $RootDir,
    "--export_type", "html:$CoverageDir",
    "--export_type", "cobertura:$coberturaOut",
    "--export_type", "binary:$CovBinary"
)
foreach ($l in $lanes) { $mergeArgs += @("--input_coverage", $l.Cov) }
$mergeArgs += @("--", (Join-Path $env:SystemRoot "System32\cmd.exe"), "/c", "exit 0")

$mergeRun = Start-LaneProcess -Exe $OpenCppCoveragePath -Arguments $mergeArgs -WorkDir $RootDir
$mergeProc = $mergeRun.Process
$mergeDeadline = (Get-Date).AddSeconds(300)
while ((Get-Date) -lt $mergeDeadline -and -not $mergeProc.HasExited) {
    Start-Sleep -Milliseconds 500
}
if (-not $mergeProc.HasExited) {
    Write-Host "ERROR: merge pass did not exit within 300s!" -ForegroundColor Red
    try { $mergeProc.Kill() } catch { }
    Write-Host "Shard workspaces kept for diagnosis: $ShardsRoot" -ForegroundColor Yellow
    exit 1
}
$mergeLog = Join-Path $HistoryDir "merge-opencppcoverage.log"
try {
    @($mergeRun.StdOut.Result, $mergeRun.StdErr.Result) |
        Where-Object { $_ } | Set-Content -Encoding UTF8 $mergeLog
}
catch { }
if (-not (Test-Path -LiteralPath $coberturaOut)) {
    Write-Host "ERROR: merge pass did not produce coverage.xml!" -ForegroundColor Red
    Write-Host "Shard workspaces kept for diagnosis: $ShardsRoot" -ForegroundColor Yellow
    exit 1
}
try {
    [xml]$mergeCheck = Get-Content -LiteralPath $coberturaOut -Raw
    if ([int]$mergeCheck.coverage.'lines-valid' -le 0) {
        Write-Host "ERROR: merged coverage.xml has zero lines-valid (empty report)!" -ForegroundColor Red
        Write-Host "Shard workspaces kept for diagnosis: $ShardsRoot" -ForegroundColor Yellow
        exit 1
    }
}
catch {
    Write-Host "ERROR: merged coverage.xml is unparsable: $_" -ForegroundColor Red
    Write-Host "Shard workspaces kept for diagnosis: $ShardsRoot" -ForegroundColor Yellow
    exit 1
}
# 合并完成且产物合格：分片工作目录（含中间 .cov/.xml/日志）不再需要
Remove-Item -LiteralPath $ShardsRoot -Recurse -Force -ErrorAction SilentlyContinue

# --- Step 3.5: Prune old .cov binaries ---
# delta 比较只依赖 summary.csv（Step 5），不读 .cov；旧 .cov 纯属占磁盘，
# 保留最近 $CovKeepCount 份（含本次刚生成的）即可。
if ($CovKeepCount -gt 0) {
    $staleCov = @(Get-ChildItem -Path $HistoryDir -Filter "*.cov" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -Skip $CovKeepCount)
    foreach ($f in $staleCov) { Remove-Item -LiteralPath $f.FullName -Force }
    if ($staleCov.Count -gt 0) {
        Write-Host ("Pruned {0} old .cov file(s), kept newest {1}." -f $staleCov.Count, $CovKeepCount) -ForegroundColor DarkGray
    }
}

# --- Step 4: Parse Cobertura XML for summary ---
Write-SummaryLine ""
Write-SummaryLine "=== Coverage Summary ($CoverageType coverage) ===" -Color Cyan

$coberturaPath = Join-Path $CoverageDir "coverage.xml"
if (-not (Test-Path $coberturaPath)) {
    Write-Host "ERROR: coverage.xml not found at '$coberturaPath'" -ForegroundColor Red
    exit 1
}
[xml]$xml = Get-Content $coberturaPath

# Files excluded from coverage statistics
$excludedFiles = @("main.cpp", "CreateDump.cpp", "LuaSyntaxChecker.cpp", "Sql.cpp", "MySqlDb.cpp")

# Strip everything up to and including "src\" so rows show source-relative paths.
# CLI 源码位于 cli\src\，先匹配并保留 cli\ 前缀，避免与引擎同名文件混淆。
function Get-SourceRelativePath {
    param([string]$Path)
    $cliMarker = "cli\src\"
    $idx = $Path.IndexOf($cliMarker, [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return "cli\" + $Path.Substring($idx + $cliMarker.Length) }
    $marker = "src\"
    $idx = $Path.IndexOf($marker, [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return $Path.Substring($idx + $marker.Length) }
    return $Path
}

$classRows = @()
$reportPaths = @{}
$allTotal = 0
$allCovered = 0

foreach ($package in $xml.coverage.packages.package) {
    foreach ($class in $package.classes.class) {
        $lines = @($class.lines.line)
        $total = $lines.Count
        $covered = @($lines | Where-Object { [int]$_.hits -gt 0 }).Count
        $allTotal += $total
        $allCovered += $covered

        $relPath = Get-SourceRelativePath ([string]$class.filename)
        $reportPaths[$relPath.ToLowerInvariant()] = $true
        if ($excludedFiles -contains (Split-Path $relPath -Leaf)) { continue }

        $classRows += [PSCustomObject]@{
            Path      = $relPath
            Total     = $total
            Covered   = $covered
            BranchSum = [double]$class.'branch-rate' * $total
        }
    }
}

# Per-class sums must match the XML root attributes
$rootTotal = [int]$xml.coverage.'lines-valid'
$rootCovered = [int]$xml.coverage.'lines-covered'
if ($allTotal -ne $rootTotal -or $allCovered -ne $rootCovered) {
    Write-SummaryLine "WARNING: per-class sum ($allCovered/$allTotal) != XML root ($rootCovered/$rootTotal)" -Color Yellow
    Write-SummaryLine "         coverage.xml structure may have changed." -Color Yellow
}

# One row per source file, aggregated from the report itself
$fileResults = @()
foreach ($g in ($classRows | Group-Object Path)) {
    $gTotal = ($g.Group | Measure-Object -Property Total -Sum).Sum
    $gCovered = ($g.Group | Measure-Object -Property Covered -Sum).Sum
    $gBranchSum = ($g.Group | Measure-Object -Property BranchSum -Sum).Sum
    $rate = if ($gTotal -gt 0) { $gCovered * 100.0 / $gTotal } else { 0.0 }
    $branchRate = if ($gTotal -gt 0) { $gBranchSum * 100.0 / $gTotal } else { 0.0 }
    $fileResults += [PSCustomObject]@{
        Path       = $g.Name
        Dir        = Split-Path $g.Name -Parent
        Total      = $gTotal
        Covered    = $gCovered
        Rate       = $rate
        BranchRate = $branchRate
    }
}

$sumTotal = ($fileResults | Measure-Object -Property Total -Sum).Sum
$sumCovered = ($fileResults | Measure-Object -Property Covered -Sum).Sum
$grandTotal = if ($null -eq $sumTotal) { 0 } else { $sumTotal }
$grandCovered = if ($null -eq $sumCovered) { 0 } else { $sumCovered }
$overallPct = if ($grandTotal -gt 0) { [math]::Round($grandCovered * 100.0 / $grandTotal, 1) } else { 0 }

# Group by source directory: worst group first, worst file first inside each group
$dirStats = foreach ($g in ($fileResults | Group-Object Dir)) {
    $dTotal = ($g.Group | Measure-Object -Property Total -Sum).Sum
    $dCovered = ($g.Group | Measure-Object -Property Covered -Sum).Sum
    $dRate = if ($dTotal -gt 0) { $dCovered * 100.0 / $dTotal } else { 0.0 }
    [PSCustomObject]@{
        Dir     = $g.Name
        Group   = $g.Group
        Total   = $dTotal
        Covered = $dCovered
        Rate    = $dRate
    }
}

foreach ($d in ($dirStats | Sort-Object Rate)) {
    $dirLabel = if ([string]::IsNullOrEmpty($d.Dir)) { "(root)" } else { $d.Dir }
    Write-SummaryLine ""
    Write-SummaryLine "--- $dirLabel ---" -Color Cyan
    foreach ($r in ($d.Group | Sort-Object Rate)) {
        $bar = ""
        $filled = [math]::Round($r.Rate / 10)
        for ($i = 0; $i -lt 10; $i++) {
            if ($i -lt $filled) { $bar += "#" } else { $bar += "." }
        }
        $color = if ($r.Rate -ge 90) { "Green" }
                 elseif ($r.Rate -ge 75) { "Yellow" }
                 else { "DarkRed" }
        if ($CoverageType -eq "branch") {
            Write-SummaryLine ("  {0,-32} {1,4}/{2,-6} {3,6}%  br {4,5}%  [{5}]" -f $r.Path, $r.Covered, $r.Total, [math]::Round($r.Rate, 1), [math]::Round($r.BranchRate, 1), $bar) -Color $color
        } else {
            Write-SummaryLine ("  {0,-32} {1,4}/{2,-6} {3,6}%  [{4}]" -f $r.Path, $r.Covered, $r.Total, [math]::Round($r.Rate, 1), $bar) -Color $color
        }
    }
    $dPct = if ($d.Total -gt 0) { [math]::Round($d.Covered * 100.0 / $d.Total, 1) } else { 0 }
    Write-SummaryLine ("  {0,-32} {1,4}/{2,-6} {3,6}%" -f "[$dirLabel subtotal]", $d.Covered, $d.Total, $dPct) -Color DarkCyan
}

Write-SummaryLine ""
Write-SummaryLine ("{0,-32} {1,4}/{2,-6} {3,6}%" -f "[Overall]", $grandCovered, $grandTotal, $overallPct) -Color White
Write-SummaryLine ("  {0} file(s) in report" -f $fileResults.Count) -Color DarkGray
if ($CoverageType -eq "branch") {
    $branchesValid = [int]$xml.coverage.'branches-valid'
    $branchesCovered = [int]$xml.coverage.'branches-covered'
    $branchPct = if ($branchesValid -gt 0) { [math]::Round($branchesCovered * 100.0 / $branchesValid, 1) } else { 0 }
    Write-SummaryLine ("  {0,-32} {1,4}/{2,-6} {3,6}%" -f "[Branch overall]", $branchesCovered, $branchesValid, $branchPct) -Color White
}

# Source files missing from the report are not instrumented and stay out of the stats
$treeFiles = @()
$treeFiles += Get-ChildItem -Path $sourcePath -Filter "*.cpp" -Recurse -ErrorAction SilentlyContinue
$treeFiles += Get-ChildItem -Path $sourcePath -Filter "*.h"   -Recurse -ErrorAction SilentlyContinue
$treeFiles += Get-ChildItem -Path $sourcePath -Filter "*.inl" -Recurse -ErrorAction SilentlyContinue

# CLI 源码（cli\src）也纳入统计，扩展名为 .cc / .h
$treeFiles += Get-ChildItem -Path $cliSourcePath -Filter "*.cc" -Recurse -ErrorAction SilentlyContinue
$treeFiles += Get-ChildItem -Path $cliSourcePath -Filter "*.h"  -Recurse -ErrorAction SilentlyContinue

$notInstrumented = @()
foreach ($f in $treeFiles) {
    $rel = Get-SourceRelativePath $f.FullName
    if (-not $reportPaths.ContainsKey($rel.ToLowerInvariant())) { $notInstrumented += $rel }
}
$notInstrumented = @($notInstrumented | Sort-Object)
Write-SummaryLine ""
Write-SummaryLine ("Not instrumented: {0} file(s) (excluded from coverage stats)" -f $notInstrumented.Count) -Color DarkGray
if ($ListNotInstrumented) {
    foreach ($rel in $notInstrumented) { Write-SummaryLine "  $rel" -Color DarkGray }
}

# Full list always lands in a file (fixed name, overwritten each run)
$NotInstrumentedFile = Join-Path $HistoryDir "not-instrumented.txt"
$niLines = @("# Not instrumented ($($notInstrumented.Count) file(s)) - $Today - excluded from coverage stats")
$niLines += $notInstrumented
$niLines | Out-File -FilePath $NotInstrumentedFile -Encoding utf8
Write-SummaryLine ("Not-instrumented list: {0}" -f $NotInstrumentedFile) -Color DarkGray

$lineRate = $overallPct
$linesValid = $grandTotal
$linesCovered = $grandCovered

# --- Step 5: Append to summary.csv ---
$delta = "n/a"
$csvHeader = "date,total_lines,covered_lines,coverage_pct,delta,method"
if (-not (Test-Path $SummaryCsv)) {
    $csvHeader | Out-File -FilePath $SummaryCsv -Encoding utf8
} else {
    $firstLine = Get-Content $SummaryCsv | Select-Object -First 1
    if ($firstLine -notmatch ",method") {
        $legacyCsv = Join-Path $HistoryDir ("summary-legacy-" + (Get-Date -Format "yyyy-MM-dd") + ".csv")
        Move-Item -Path $SummaryCsv -Destination $legacyCsv -Force
        Write-Host "Archived previous summary: $legacyCsv" -ForegroundColor DarkGray
        $csvHeader | Out-File -FilePath $SummaryCsv -Encoding utf8
    }
}
# delta 只与同构建配置的上一条比较：不同配置的行映射不同，跨配置比较
# 会得到无意义的大跳水。method 列记录采集配置，切换配置后首条即新基线。
# 本脚本现在合并引擎测试与 CLI 测试的覆盖率，故 method 标注 -merge；
# 合并后的首条记录即新的合并基线，不会与旧引擎单测基线作差。
$method = "xml-merge-$($BuildConfig.ToLower())"
$prevLine = Get-Content $SummaryCsv |
    Where-Object { ($_ -split ",")[-1] -eq $method } |
    Select-Object -Last 1
if ($prevLine) {
    $prevFields = $prevLine -split ","
    if ($prevFields.Count -ge 4) {
        $prevPct = [double]$prevFields[3]
        $deltaVal = [math]::Round($lineRate - $prevPct, 1)
        if ($deltaVal -ge 0) { $delta = "+" + $deltaVal.ToString("0.0") } else { $delta = $deltaVal.ToString("0.0") }
    }
}

$dateStr = Get-Date -Format "yyyy-MM-dd"
"$dateStr,$linesValid,$linesCovered,$([math]::Round($lineRate, 1).ToString("0.0")),$delta,$method" | Out-File -FilePath $SummaryCsv -Append -Encoding utf8

# --- Step 5.5: Prune old summary history ---
# csv 与每轮一份的 summary-<时间戳>.txt 都只保留最近 $SummaryKeepCount 条，
# 避免历史记录无限增长；刚写入的本轮记录在保留窗口内，不受影响。
if ($SummaryKeepCount -gt 0) {
    $csvLines = @(Get-Content -LiteralPath $SummaryCsv)
    if ($csvLines.Count -gt $SummaryKeepCount + 1) {
        $keep = @($csvLines | Select-Object -First 1) +
                @($csvLines | Select-Object -Skip 1 | Select-Object -Last $SummaryKeepCount)
        $keep | Out-File -FilePath $SummaryCsv -Encoding utf8
        Write-Host ("Pruned {0} old row(s) from summary.csv, kept newest {1}." -f ($csvLines.Count - $keep.Count), $SummaryKeepCount) -ForegroundColor DarkGray
    }
    # 只清理本脚本产出的 summary-<时间戳>.txt（时间戳后缀为 _），不碰其他 summary-*.txt
    # （如 run-integration-tests.ps1 写的 summary-e2e-sample.txt）。
    $staleLogs = @(Get-ChildItem -Path $HistoryDir -Filter "summary-*.txt" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "^summary-\d{4}-\d{2}-\d{2}_\d{6}\.txt$" } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -Skip $SummaryKeepCount)
    foreach ($f in $staleLogs) { Remove-Item -LiteralPath $f.FullName -Force }
    if ($staleLogs.Count -gt 0) {
        Write-Host ("Pruned {0} old summary txt file(s), kept newest {1}." -f $staleLogs.Count, $SummaryKeepCount) -ForegroundColor DarkGray
    }
}

# --- Step 6: Show delta ---
$deltaColor = if ($delta.StartsWith("+") -and $delta -ne "+0.0") { "Green" }
              elseif ($delta.StartsWith("-")) { "Red" }
              else { "Gray" }
Write-SummaryLine ""
if ($delta -eq "n/a") {
    Write-SummaryLine "Delta: n/a (首个 $BuildConfig 采集基线)" -Color Yellow
} else {
    Write-SummaryLine "Delta: $delta%" -Color $deltaColor
}
Write-SummaryLine "Report: $CoverageDir\index.html" -Color DarkGray
Write-SummaryLine "Cobertura: $coberturaPath" -Color DarkGray
Write-SummaryLine "Binary: $CovBinary" -Color DarkGray

$SummaryLogLines | Out-File -FilePath $SummaryLog -Encoding utf8
Write-Host "Summary log: $SummaryLog" -ForegroundColor DarkGray

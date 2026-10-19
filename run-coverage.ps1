# run-coverage.ps1 - Collect code coverage for tests
#
# Usage:
#   powershell -File run-coverage.ps1                    # line coverage (default)
#   powershell -File run-coverage.ps1 -CoverageType branch  # branch coverage
#   powershell -File run-coverage.ps1 -ListNotInstrumented  # also list files absent from the report
#   powershell -File run-coverage.ps1 -CovKeepCount 3       # 只保留最近 3 份 .cov（0 = 不清理）
#
# Requires: OpenCppCoverage (https://github.com/OpenCppCoverage/OpenCppCoverage)
#           installed and on PATH, or set $OpenCppCoveragePath below.
#
# What it does:
#   1. Builds tests (Release|Win32 — 生产同构配置；Debug 构建的
#      测试程序内含 TestConfigGate，运行即报错，故默认禁止)。
#      引擎测试（tests）与 CLI 测试（cli\tests）分别构建。
#   2. Runs engine tests then CLI tests under OpenCppCoverage
#      （分别运行，CLI 那轮以 --input_coverage 合并引擎覆盖率）
#   3. Generates HTML report in coverage_report/
#   4. Exports Cobertura XML for CI
#   5. Saves binary .cov to coverage_history/ (pruned to newest -CovKeepCount)
#   6. Appends summary to coverage_history/summary.csv
#   7. Shows coverage delta vs previous run (同配置基线比较)
#   8. Saves the Coverage Summary text to
#      coverage_history/summary-<timestamp>.txt
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
    [int]$CovKeepCount = 5
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
$testProjects = @( "tests\unit\tests.vcxproj", "cli\tests\jianghu_cli_tests.vcxproj" )
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

# --- Step 3: Run coverage ---
Write-Host "`n=== Running tests with coverage ===" -ForegroundColor Cyan

$sourcePath = Join-Path $RootDir $ServerSrc
$cliSourcePath = Join-Path $RootDir $CliSrc

# 分支覆盖率开关（仅在请求时添加）
$branchArg = @()
if ($CoverageType -eq "branch") {
    $branchArg = @("--branch_coverage")
}

# 第一次：只跑引擎测试，导出二进制覆盖率。
$engineCov = Join-Path $CoverageDir "engine.cov"
$engineCovArgs = $branchArg + @(
    "--sources", "`"$sourcePath\*`"",
    "--modules", "`"$TestExe`"",
    "--working_dir", "`"$RootDir`"",
    "--export_type", "binary:$engineCov",
    "--"
) + @("`"$TestExe`"")

$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $OpenCppCoveragePath @engineCovArgs
$ErrorActionPreference = $prevEAP
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Engine test execution failed!" -ForegroundColor Red
    exit 1
}

# 第二次：跑 CLI 测试，并以 --input_coverage 合并引擎的覆盖率，
# 使最终 HTML/Cobertura/二进制报告同时包含两套测试的结果。
$covArgs = $branchArg + @(
    "--sources", "`"$sourcePath\*`"",
    "--sources", "`"$cliSourcePath\*`"",
    "--modules", "`"$CliTestExe`"",
    "--working_dir", "`"$RootDir`"",
    "--input_coverage", "`"$engineCov`"",
    "--export_type", "html:$CoverageDir",
    "--export_type", "cobertura:$CoverageDir\coverage.xml",
    "--export_type", "binary:$CovBinary",
    "--"
) + @("`"$CliTestExe`"")

$ErrorActionPreference = "Continue"
& $OpenCppCoveragePath @covArgs
$ErrorActionPreference = $prevEAP
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: CLI test execution failed!" -ForegroundColor Red
    exit 1
}

# 合并完成，中间文件不再需要
Remove-Item -LiteralPath $engineCov -Force -ErrorAction SilentlyContinue

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

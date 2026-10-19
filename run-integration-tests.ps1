# run-integration-tests.ps1 — 集成测试入口（lua-sample 单人主线 E2E）
#
# 与单元测试入口 run-coverage.ps1 并列。作用：
#   1. 构建 Release|Win32 的服务器 bin\jianghu.exe 与 CLI bin\cli\jianghu_cli.exe；
#   2. 用 tests\integration\config.ini 启动真实服务器（服务端命令行 --config 指定配置，
#      不再临时改写 data\config.ini），独立测试库 + 独立端口；
#   3. 依次运行 tests\integration\scenarios\ 下的 register.sce、sample_mainline.sce
#      、sample_social.sce 与 sample_house.sce；
#   4. 在 OpenCppCoverage 下采集 src 行覆盖，导出 Cobertura / HTML / binary；
#   5. 在控制台输出测试结果、行覆盖率与「未命中函数」清单；清单过长时只报数字，
#      完整清单写入 coverage_history\e2e-sample-unhit-functions.txt；
#   6. 集成测试失败时只输出测试结果与失败原因，不展示覆盖率与未命中函数
#      （避免覆盖率刷屏淹没失败信息；报告文件仍会导出，便于排查）。
#
# 依赖：MSBuild（VS）、OpenCppCoverage、本地 MySQL（服务 "MySQLa"，127.0.0.1:3306）。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -SkipBuild -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -CliExe C:\tmp\jhcli.exe
#
# 说明：
#   - 使用独立测试库（tests\integration\config.ini 的 game_e2e_sample），不污染业务库；
#     每次使用唯一角色名，因此无需清库。环境提供 mysql.exe 时可用 -ResetDb 强制重建。
#   - 覆盖率与「未命中函数」都是近似口径，详见 docs\e2e_sample_test.md。

param(
    # CLI 可执行文件；默认 bin\cli\jianghu_cli.exe
    [string]$CliExe = "",
    # 跳过 MSBuild 构建（直接使用现有 bin\jianghu.exe / CLI）
    [switch]$SkipBuild,
    # 跳过 OpenCppCoverage（快速调试剧本，不出覆盖率报告）
    [switch]$NoCoverage,
    # 运行前重建测试库（需要 mysql.exe 在 PATH 或 -MysqlExe）
    [switch]$ResetDb,
    [string]$MysqlExe = "mysql",
    # 单个 wait/assert 默认超时（毫秒）
    [int]$StepTimeout = 20000,
    # 等待服务器端口就绪的超时（秒）。首次在 OpenCppCoverage 下加载 PDB 可能较慢
    [int]$ServerReadyTimeoutSec = 180,
    # 长时间无输出时的心跳间隔（秒）
    [int]$HeartbeatSec = 10,
    # 控制台最多逐条打印多少个未命中函数；超出则只报数字
    [int]$UnhitListLimit = 60
)

$ErrorActionPreference = "Stop"

# --- 路径 -------------------------------------------------------------------
$RepoRoot = $PSScriptRoot
$IntDir = Join-Path $RepoRoot "tests\integration"
$ConfPath = Join-Path $IntDir "config.ini"
$ScenarioDir = Join-Path $IntDir "scenarios"
$CliConf = Join-Path $IntDir "cli.conf"
$ServerProj = Join-Path $RepoRoot "jianghu.vcxproj"
$CliProj = Join-Path $RepoRoot "cli\jianghu_cli.vcxproj"
$ServerExe = Join-Path $RepoRoot "bin\jianghu.exe"
$ReportDir = Join-Path $RepoRoot "coverage_report_e2e_sample"
$CovBin = Join-Path $RepoRoot "coverage_history\e2e_sample.cov"
$SummaryFile = Join-Path $RepoRoot "coverage_history\summary-e2e-sample.txt"
$UnhitFile = Join-Path $RepoRoot "coverage_history\e2e-sample-unhit-functions.txt"
$VarsFile = Join-Path $env:TEMP "e2e_sample.vars"
$RegReport = Join-Path $env:TEMP "e2e_register.report"
$MainReport = Join-Path $env:TEMP "e2e_sample_mainline.report"
$SocialReport = Join-Path $env:TEMP "e2e_sample_social.report"
$HouseReport = Join-Path $env:TEMP "e2e_sample_house.report"
$OccExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"

# ===========================================================================
# 源码扫描：近似提取「未命中函数」
# ---------------------------------------------------------------------------
# OpenCppCoverage 的 Cobertura 导出不含函数级数据（<class> 的 <methods/> 是空的），
# 这里按本仓库的 Allman 风格（函数体 '{' 独占一行）识别函数定义，再结合行命中数据，
# 把「函数体内没有任何一行被命中」的函数判为未命中。
#
# 已知局限：单行风格函数体（`void f() {`）、模板函数、lambda 不计入；预处理器
# 未编译分支也会被计入；编译器内联可能让实际执行过的函数显示为 0；只在 Cobertura
# 出现过的文件里扫描，因此不会把未插桩的文件误报为未命中。
# ===========================================================================

function ConvertTo-CodeLine {
    param(
        [string]$Line,
        [ref]$InBlockComment
    )
    $s = $Line
    if ($InBlockComment.Value) {
        $end = $s.IndexOf('*/')
        if ($end -lt 0) { return '' }
        $s = $s.Substring($end + 2)
        $InBlockComment.Value = $false
    }
    while ($true) {
        $st = $s.IndexOf('/*')
        if ($st -lt 0) { break }
        $en = $s.IndexOf('*/', $st + 2)
        if ($en -lt 0) {
            $InBlockComment.Value = $true
            $s = $s.Substring(0, $st)
            break
        }
        $s = $s.Substring(0, $st) + ' ' + $s.Substring($en + 2)
    }
    # 字符字面量先于字符串字面量，避免 '"' 被误当字符串起点
    $s = [regex]::Replace($s, "'(?:\\.|[^'\\])*'", "''")
    $s = [regex]::Replace($s, '"(?:\\.|[^"\\])*"', '""')
    $ci = $s.IndexOf('//')
    if ($ci -ge 0) { $s = $s.Substring(0, $ci) }
    return $s
}

# 控制语句 / 非函数块开头关键字
$script:NonFunctionHeads = 'if|for|while|switch|catch|else|do|try|return|namespace|class|struct|union|enum|extern|typedef|using|case|default|static_assert|assert'

function Get-SourceFunctions {
    param([string[]]$Lines)

    $n = $Lines.Count
    $code = New-Object string[] $n
    $inBlock = $false
    for ($i = 0; $i -lt $n; $i++) {
        $code[$i] = ConvertTo-CodeLine -Line $Lines[$i] -InBlockComment ([ref]$inBlock)
    }

    $result = New-Object System.Collections.ArrayList
    for ($i = 0; $i -lt $n; $i++) {
        if ($code[$i].Trim() -ne '{') { continue }

        # 向前收集签名，遇到语句边界即停
        $sig = New-Object System.Collections.ArrayList
        $k = $i - 1
        while ($k -ge 0 -and $sig.Count -lt 15) {
            $pt = $code[$k].Trim()
            if ($pt -eq '') { break }
            if ($pt.StartsWith('#')) { break }
            if ($pt.EndsWith(';') -or $pt.EndsWith('{') -or $pt.EndsWith('}')) { break }
            if ($pt -match '^(public|private|protected)\s*:') { break }
            $sig.Insert(0, $pt)
            if ($pt -match '\)\s*[\w\s]*$') { break }
            $k--
        }
        if ($sig.Count -eq 0) { continue }

        $text = ($sig -join ' ').Trim()

        if ($text -match '^template\s*<') {
            $text = ($text -replace '^template\s*<[^<>]*(?:<[^<>]*>[^<>]*)*>\s*', '').Trim()
            if ($text -eq '') { continue }
        }

        if ($text -match "^($script:NonFunctionHeads)\b") { continue }
        if (-not ($text -match '\)\s*[\w\s]*$')) { continue }

        $pIdx = $text.IndexOf('(')
        if ($pIdx -lt 0) { continue }
        if ($text.Substring(0, $pIdx) -match '=') { continue }

        $m = [regex]::Match($text, '(operator\s*[^\s(]+|~?[A-Za-z_]\w*(?:::[~A-Za-z_]\w*)*)\s*\(')
        if (-not $m.Success) { continue }
        $name = ($m.Groups[1].Value -replace '\s+', '')

        # 匹配函数体结束
        $d = 1
        $end = $i
        for ($j = $i + 1; $j -lt $n; $j++) {
            $d += ([regex]::Matches($code[$j], '\{')).Count
            $d -= ([regex]::Matches($code[$j], '\}')).Count
            if ($d -le 0) { $end = $j; break }
        }

        [void]$result.Add([pscustomobject]@{
                Name  = $name
                Start = $i + 1
                End   = $end + 1
            })
    }
    return $result
}

function ConvertTo-SrcRelative {
    param([string]$FileName)
    $idx = $FileName.IndexOf('src\', [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return 'src\' + $FileName.Substring($idx + 4) }
    $idx = $FileName.IndexOf('src/', [System.StringComparison]::OrdinalIgnoreCase)
    if ($idx -ge 0) { return 'src/' + $FileName.Substring($idx + 4) }
    return $FileName
}

function Get-UnhitFunctionReport {
    param([string]$CoberturaPath)

    $srcRoot = Join-Path $RepoRoot 'src'
    if (-not (Test-Path $CoberturaPath)) { throw "找不到 Cobertura 报告：$CoberturaPath" }

    [xml]$x = Get-Content -LiteralPath $CoberturaPath -Encoding UTF8
    $classes = @($x.coverage.packages.package.classes.class)

    $unhit = New-Object System.Collections.ArrayList
    $totalFunctions = 0
    $hitFunctions = 0
    $scannedFiles = 0

    foreach ($c in $classes) {
        $rel = ConvertTo-SrcRelative ([string]$c.filename)
        if ($rel -notmatch '^src[\\/]') { continue }

        $hits = @{}
        foreach ($ln in @($c.lines.line)) {
            $hits[[int]$ln.number] = [int]$ln.hits
        }

        $filePath = Join-Path $srcRoot ($rel -replace '^src[\\/]', '')
        if (-not (Test-Path -LiteralPath $filePath)) { continue }
        $scannedFiles++

        $lines = [System.IO.File]::ReadAllLines($filePath)
        if ($lines.Count -eq 0) { continue }

        foreach ($fn in (Get-SourceFunctions -Lines $lines)) {
            $anyReported = $false
            $anyHit = $false
            for ($ln = $fn.Start; $ln -le $fn.End; $ln++) {
                if ($hits.ContainsKey($ln)) {
                    $anyReported = $true
                    if ($hits[$ln] -gt 0) { $anyHit = $true; break }
                }
            }
            if (-not $anyReported) { continue }   # 无插桩数据，无法判定
            $totalFunctions++
            if ($anyHit) { $hitFunctions++ }
            else {
                [void]$unhit.Add([pscustomobject]@{
                        File  = $rel
                        Name  = $fn.Name
                        Start = $fn.Start
                        End   = $fn.End
                        Lines = $fn.End - $fn.Start + 1
                    })
            }
        }
    }

    return [pscustomobject]@{
        ScannedFiles   = $scannedFiles
        TotalFunctions = $totalFunctions
        HitFunctions   = $hitFunctions
        UnhitFunctions = $unhit
    }
}

# ===========================================================================
# 辅助
# ===========================================================================

function Get-IniValue([string]$path, [string]$section, [string]$key) {
    $cur = ""
    foreach ($line in [System.IO.File]::ReadAllLines($path, [System.Text.Encoding]::UTF8)) {
        $t = $line.Trim()
        if ($t -match '^\[(.+)\]$') { $cur = $Matches[1]; continue }
        if ($cur -eq $section -and $t -match "^$key\s*=\s*(.*)$") {
            return $Matches[1].Trim()
        }
    }
    return ""
}

function Get-MSBuildPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $p = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null
        if ($p) { return @($p)[0].Trim() }
    }
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "找不到 MSBuild（请安装 Visual Studio 或把 msbuild.exe 加入 PATH）"
}

function Wait-Port([int]$p, [int]$timeoutSec) {
    $start = Get-Date
    $nextBeat = $start.AddSeconds($HeartbeatSec)
    $deadline = $start.AddSeconds($timeoutSec)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection -ComputerName 127.0.0.1 -Port $p -WarningAction SilentlyContinue).TcpTestSucceeded) {
            return $true
        }
        $now = Get-Date
        if ($now -ge $nextBeat) {
            Write-Host ("   ... 等待端口 $p 就绪（已 {0}s，上限 {1}s）" -f `
                    [int]($now - $start).TotalSeconds, $timeoutSec)
            $nextBeat = $now.AddSeconds($HeartbeatSec)
        }
        Start-Sleep -Milliseconds 300
    }
    return $false
}

function Stop-ServerGraceful() {
    foreach ($p in (Get-Process jianghu -ErrorAction SilentlyContinue)) {
        try { [void]$p.CloseMainWindow() } catch { }
    }
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-Process jianghu -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Milliseconds 500
    }
    Get-Process jianghu -ErrorAction SilentlyContinue | Stop-Process -Force
}

# 给子进程参数加引号（Start-Process / ProcessStartInfo.Arguments 只接受单个字符串）
function Quote-Arg([string]$s) {
    if ($s -match '[\s"]') { return '"' + ($s -replace '"', '\"') + '"' }
    return $s
}

# 无窗口启动控制台子进程，避免弹出/抢占前台的控制台窗口。
# CreateNoWindow = true 会带上 CREATE_NO_WINDOW，控制台程序也不会分配新控制台。
function New-HiddenStartInfo([string]$Exe, [string[]]$Arguments, [string]$WorkingDirectory) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = (($Arguments | ForEach-Object { Quote-Arg $_ }) -join ' ')
    $psi.WorkingDirectory = $WorkingDirectory
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    # 子进程（CLI / OpenCppCoverage）输出 UTF-8 字节；重定向流默认按
    # [Console]::OutputEncoding（本机多为 cp936）解码，会导致中文乱码。
    # 这里按流固定为 UTF-8，与 [Console]::OutputEncoding 解耦，避免影响 Write-Host。
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    return $psi
}

# 无窗口启动长驻子进程（用于 OpenCppCoverage）；输出由调用方在退出后取回
function Start-HiddenProcess {
    param([string]$Exe, [string[]]$Arguments, [string]$WorkingDirectory)

    $psi = New-HiddenStartInfo $Exe $Arguments $WorkingDirectory
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

# ===========================================================================
# 准备
# ===========================================================================

foreach ($p in @($ConfPath, $CliConf, $ServerProj)) {
    if (-not (Test-Path $p)) { throw "缺少文件：$p" }
}

$Port = [int](Get-IniValue $ConfPath "server" "port")
$DbName = Get-IniValue $ConfPath "database" "name"
if ($DbName -eq "") { throw "无法从 $ConfPath 解析 [database] name" }
if ($CliExe -eq "") { $CliExe = Join-Path $RepoRoot "bin\cli\jianghu_cli.exe" }

Write-Host "== lua-sample 集成测试（单人主线 E2E）=="
Write-Host "repo     : $RepoRoot"
Write-Host "config   : $ConfPath"
Write-Host "port     : $Port   db: $DbName"

# --- 构建 -------------------------------------------------------------------
if (-not $SkipBuild) {
    $msb = Get-MSBuildPath
    Write-Host "-- 构建服务器与 CLI（Release|Win32）"
    foreach ($proj in @($ServerProj, $CliProj)) {
        & $msb $proj /p:Configuration=Release /p:Platform=Win32 /nologo /verbosity:minimal
        if ($LASTEXITCODE -ne 0) { throw "构建失败：$proj" }
    }
}

foreach ($p in @($ServerExe, $CliExe)) {
    if (-not (Test-Path $p)) { throw "缺少文件：$p（可去掉 -SkipBuild 重新构建）" }
}
if (-not $NoCoverage -and -not (Test-Path $OccExe)) {
    throw "找不到 OpenCppCoverage：$OccExe（可用 -NoCoverage 跳过）"
}

# --- 数据库（可选重建） -----------------------------------------------------
if ($ResetDb) {
    Write-Host "-- 重建测试库 $DbName"
    & $MysqlExe -h 127.0.0.1 -u root -proot -e "DROP DATABASE IF EXISTS $DbName; CREATE DATABASE $DbName DEFAULT CHARACTER SET utf8;"
    if ($LASTEXITCODE -ne 0) { Write-Warning "mysql 重建库失败，改用唯一角色名策略继续。" }
}

# --- 生成 vars（唯一角色名 + 邮箱） ------------------------------------------------
# 角色名长度限制为 2~10 字节（LoginRegister::check_legal_name / make_body），
# 用 7 位十六进制后缀凑成 10 字节，避免与历史账号重名。
$Suffix = '{0:x7}' -f ([DateTime]::UtcNow.Ticks -band 0xFFFFFFF)
$Name = "e2e$Suffix"
$Email = "e2e$Suffix@example.com"
"name=$Name`npass=e2e1234`nemail=$Email" | Set-Content -Encoding ASCII $VarsFile

# ===========================================================================
# 运行
# ===========================================================================

$serverProc = $null
$occRun = $null
$status = "fail"
$failReason = ""
$stepCounts = @{}

# 事件 action 跑在独立 runspace：共享队列 + 共享状态，
# 每收到一行就立刻打到控制台。
$script:EmitCliLine = {
    $line = $EventArgs.Data
    if ($null -eq $line -or $line -eq '') { return }
    $Event.MessageData.Queue.Enqueue($line)
    $Event.MessageData.State.Last = $line
    $Event.MessageData.State.Count = [int]$Event.MessageData.State.Count + 1
    Write-Host "   $line"
}

# 无窗口运行剧本 CLI：stdout/stderr 边到边打，超过 $HeartbeatSec 无输出
# 时补一条心跳（含已运行秒数、收到行数、最后一行），避免整段剧本期间
# 控制台毫无反馈、分不清「正在跑」与「已卡死」。
# 返回 CLI 退出码；PASS 步数解析进 $stepCounts。
function Invoke-Scenario([string]$file, [string]$report) {
    $psi = New-HiddenStartInfo -Exe $CliExe -WorkingDirectory $RepoRoot -Arguments @(
        "--config", $CliConf,
        "--script", (Join-Path $ScenarioDir $file),
        "--vars", $VarsFile,
        "--step-timeout", "$StepTimeout",
        "--report", $report
    )
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()

    $received = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()
    $state = [hashtable]::Synchronized(@{ Last = ""; Count = 0 })
    $shared = @{ Queue = $received; State = $state }
    $outName = "e2e-cli-out-$file"
    $errName = "e2e-cli-err-$file"
    $jobs = @()
    $done = $false
    try {
        $jobs += Register-ObjectEvent -InputObject $p -EventName OutputDataReceived `
            -SourceIdentifier $outName -MessageData $shared -Action $script:EmitCliLine
        $jobs += Register-ObjectEvent -InputObject $p -EventName ErrorDataReceived `
            -SourceIdentifier $errName -MessageData $shared -Action $script:EmitCliLine
        $p.BeginOutputReadLine()
        $p.BeginErrorReadLine()

        $started = Get-Date
        $nextBeat = $started.AddSeconds($HeartbeatSec)
        while (-not $p.WaitForExit(500)) {
            $now = Get-Date
            if ($now -ge $nextBeat) {
                $nextBeat = $now.AddSeconds($HeartbeatSec)
                $last = [string]$state.Last
                if ($last -eq '') { $last = "（尚无输出）" }
                if ($last.Length -gt 60) { $last = $last.Substring(0, 60) + "..." }
                Write-Host ("   ... 已运行 {0}s，收到 {1} 行，最后：{2}" -f `
                        [int]($now - $started).TotalSeconds, [int]$state.Count, $last)
            }
        }
        $p.WaitForExit()   # 等异步读取收尾，保证输出队列完整
        $code = $p.ExitCode
        $done = $true
    }
    finally {
        foreach ($name in @($outName, $errName)) {
            Unregister-Event -SourceIdentifier $name -ErrorAction SilentlyContinue
        }
        foreach ($j in $jobs) { Remove-Job -Job $j -Force -ErrorAction SilentlyContinue }
        if (-not $done) {
            # 异常路径上不留孤儿 CLI，否则它会占着测试角色继续跑
            try { if (-not $p.HasExited) { $p.Kill() } } catch { }
        }
        $p.Dispose()
    }

    $out = @()
    $line = ""
    while ($received.TryDequeue([ref]$line)) { $out += $line }

    $m = [regex]::Match(($out -join "`n"), '\[scenario\] PASS:\s*(\d+)\s*steps')
    if ($m.Success) { $script:stepCounts[$file] = [int]$m.Groups[1].Value }
    return $code
}

try {
    # --- 启动服务器（--config 指向集成测试配置） -----------------------------
    Get-Process jianghu -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 1

    if ($NoCoverage) {
        $serverProc = (Start-HiddenProcess -Exe $ServerExe -WorkingDirectory $RepoRoot `
                -Arguments @("--config", $ConfPath)).Process
    }
    else {
        if (Test-Path $ReportDir) { Remove-Item $ReportDir -Recurse -Force }
        New-Item -ItemType Directory -Force -Path (Split-Path $CovBin) | Out-Null
        $occArgs = @(
            "--modules", $ServerExe,
            "--sources", (Join-Path $RepoRoot "src"),
            "--optimized_build",
            "--working_dir", $RepoRoot,
            "--export_type", "html:$ReportDir",
            "--export_type", "cobertura:$ReportDir\coverage.xml",
            "--export_type", "binary:$CovBin",
            "--",
            $ServerExe, "--config", $ConfPath
        )
        $occRun = Start-HiddenProcess -Exe $OccExe -WorkingDirectory $RepoRoot -Arguments $occArgs
        Write-Host "-- OpenCppCoverage 已启动（pid $($occRun.Process.Id)，无窗口）"
    }

    if (-not (Wait-Port $Port $ServerReadyTimeoutSec)) { throw "服务器未在端口 $Port 就绪（超时 $ServerReadyTimeoutSec 秒）" }
    Write-Host "-- 服务器就绪"

    # --- 运行剧本 -----------------------------------------------------------
    Write-Host "-- 阶段 1：注册新角色 ($Name)"
    if ((Invoke-Scenario "register.sce" $RegReport) -ne 0) { throw "register.sce 失败" }

    Write-Host "-- 阶段 2：主线剧本 sample_mainline.sce"
    if ((Invoke-Scenario "sample_mainline.sce" $MainReport) -ne 0) { throw "sample_mainline.sce 失败" }

    Write-Host "-- 阶段 3：非战斗剧本 sample_social.sce"
    if ((Invoke-Scenario "sample_social.sce" $SocialReport) -ne 0) { throw "sample_social.sce 失败" }

    Write-Host "-- 阶段 4：房屋/居民剧本 sample_house.sce"
    if ((Invoke-Scenario "sample_house.sce" $HouseReport) -ne 0) { throw "sample_house.sce 失败" }

    $status = "pass"
    Write-Host "-- 四部剧本均通过"
}
catch {
    $failReason = $_.Exception.Message
    Write-Warning "运行失败：$failReason"
}
finally {
    Stop-ServerGraceful
    if (-not $NoCoverage -and $occRun) {
        Write-Host "-- 等待 OpenCppCoverage 导出报告..."
        $occProc = $occRun.Process
        $occStart = Get-Date
        $occDeadline = $occStart.AddSeconds(180)
        $occBeat = $occStart.AddSeconds(30)
        while ((Get-Date) -lt $occDeadline) {
            if ($occProc.HasExited) { break }
            $now = Get-Date
            if ($now -ge $occBeat) {
                $occBeat = $now.AddSeconds(30)
                Write-Host ("   ... 服务器已停止，等待导出报告（已 {0}s，上限 180s）" -f `
                        [int]($now - $occStart).TotalSeconds)
            }
            Start-Sleep -Milliseconds 500
        }
        if (-not $occProc.HasExited) {
            Write-Warning "OpenCppCoverage 未退出，强制结束"
            $occProc | Stop-Process -Force
        }
        else {
            # 采集日志留档，便于排查报告异常
            $occLog = Join-Path (Split-Path $CovBin) "e2e-sample-opencppcoverage.log"
            @($occRun.StdOut.Result, $occRun.StdErr.Result) |
                Where-Object { $_ } | Set-Content -Encoding UTF8 $occLog
        }
    }
}

# ===========================================================================
# 报告
# ===========================================================================

$lines = New-Object System.Collections.ArrayList
$cov = $null

$covXml = Join-Path $ReportDir "coverage.xml"
# 失败时跳过覆盖率计算与展示：部分执行的覆盖率既无意义，又会刷屏淹没失败信息
if ($status -eq "pass" -and (-not $NoCoverage) -and (Test-Path $covXml)) {
    [xml]$xml = Get-Content $covXml -Encoding UTF8
    $totalLines = 0; $totalCovered = 0
    $zeroFiles = @{}
    $dirStat = @{}
    foreach ($pkg in $xml.coverage.packages.package) {
        foreach ($c in $pkg.classes.class) {
            $fn = [string]$c.filename
            if ($fn -notmatch '[\\/]src[\\/]') { continue }
            $lineNodes = @($c.lines.line)
            $loc = $lineNodes.Count
            if ($loc -le 0) { continue }
            $covCount = @($lineNodes | Where-Object { [int]$_.hits -gt 0 }).Count
            $totalLines += $loc
            $totalCovered += $covCount

            $rel = ConvertTo-SrcRelative $fn
            if ($covCount -eq 0) { $zeroFiles[$rel] = $loc }

            $dir = "src"
            $m = [regex]::Match($rel, '^src\\([^\\]+)')
            if ($m.Success) { $dir = $m.Groups[1].Value }
            if (-not $dirStat.ContainsKey($dir)) { $dirStat[$dir] = @(0, 0) }
            $dirStat[$dir][0] += $loc
            $dirStat[$dir][1] += $covCount
        }
    }
    if ($totalLines -gt 0) {
        $cov = [pscustomobject]@{
            Percent    = [math]::Round(100.0 * $totalCovered / $totalLines, 1)
            Covered    = $totalCovered
            Total      = $totalLines
            Dirs       = $dirStat
            ZeroFiles  = $zeroFiles
        }
    }
}

$fnReport = $null
if ($cov) {
    Write-Host "-- 扫描源码，统计未命中函数..."
    $fnReport = Get-UnhitFunctionReport -CoberturaPath $covXml
}

# --- 控制台报告 -------------------------------------------------------------
Write-Host ""
Write-Host "================ 集成测试结果 ================"
if ($status -eq "pass") { Write-Host "状态        : PASS" } else { Write-Host "状态        : FAIL" }
if ($failReason) { Write-Host "失败原因    : $failReason" }
Write-Host "测试角色    : $Name"
foreach ($f in @("register.sce", "sample_mainline.sce", "sample_social.sce", "sample_house.sce")) {
    $steps = if ($stepCounts.ContainsKey($f)) { "$($stepCounts[$f]) steps" } else { "-" }
    $mark = if ($status -eq "pass") { "PASS" } else { "FAIL" }
    Write-Host ("  {0,-16} {1}  ({2})" -f $f, $mark, $steps)
}
if ($status -ne "pass" -and -not $NoCoverage -and (Test-Path $ReportDir)) {
    Write-Host ""
    Write-Host "（集成测试失败：跳过覆盖率与未命中函数展示；报告仍在 $ReportDir）"
}

$lines += "lua-sample E2E：$status"
if ($failReason) { $lines += "reason=$failReason" }
$lines += "name=$Name"
$lines += "time=$(Get-Date -Format s)"

if ($cov) {
    Write-Host ""
    Write-Host "================ 覆盖率（src 行）=============="
    Write-Host ("行覆盖率    : {0}%（{1}/{2}）" -f $cov.Percent, $cov.Covered, $cov.Total)
    $lines += ""
    $lines += ("src 行覆盖率：{0}%（{1}/{2}）" -f $cov.Percent, $cov.Covered, $cov.Total)
    $lines += ""
    $lines += "--- 按二级目录 ---"
    foreach ($d in ($cov.Dirs.GetEnumerator() | Sort-Object { -$_.Value[0] })) {
        $sub = [math]::Round(100.0 * $d.Value[1] / [math]::Max(1, $d.Value[0]), 1)
        $row = ("  {0,-14} {1,6}/{2,-6} {3,5}%" -f $d.Key, $d.Value[1], $d.Value[0], $sub)
        Write-Host $row
        $lines += $row
    }
    $lines += ""
    $lines += ("0 命中文件（{0} 个，按 LOC 降序）——单骑 E2E 行走不到的引擎功能：" -f $cov.ZeroFiles.Count)
    foreach ($k in ($cov.ZeroFiles.GetEnumerator() | Sort-Object Value -Descending)) {
        $lines += ("  {0,5}  {1}" -f $k.Value, $k.Key)
    }
}

if ($fnReport) {
    $unhitList = @($fnReport.UnhitFunctions | Sort-Object File, Start)
    Write-Host ""
    Write-Host "================ 未命中函数 ==================="
    Write-Host ("近似统计    : 命中 {0}/{1}，未命中 {2}（扫描 {3} 个文件）" -f `
            $fnReport.HitFunctions, $fnReport.TotalFunctions, $unhitList.Count, $fnReport.ScannedFiles)

    $listOut = New-Object System.Collections.ArrayList
    [void]$listOut.Add("# lua-sample E2E 未命中函数清单（近似）")
    [void]$listOut.Add("# 由 run-integration-tests.ps1 生成，口径与局限见 docs\e2e_sample_test.md")
    [void]$listOut.Add("# 命中 $($fnReport.HitFunctions)/$($fnReport.TotalFunctions)，未命中 $($unhitList.Count)，扫描 $($fnReport.ScannedFiles) 个文件")
    [void]$listOut.Add("")
    foreach ($u in $unhitList) {
        [void]$listOut.Add(("{0}:{1}  {2}" -f $u.File, $u.Start, $u.Name))
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $UnhitFile) | Out-Null
    $listOut | Set-Content -Encoding UTF8 $UnhitFile
    Write-Host ("完整清单    : {0}" -f $UnhitFile)

    if ($unhitList.Count -le $UnhitListLimit) {
        foreach ($u in $unhitList) {
            Write-Host ("  {0}:{1}  {2}" -f $u.File, $u.Start, $u.Name)
        }
    }
    else {
        Write-Host ("（清单共 {0} 条，超过控制台上限 {1}，仅报数字；完整清单见上）" -f $unhitList.Count, $UnhitListLimit)
    }

    $lines += ""
    $lines += ("未命中函数：{0}/{1}（未命中 {2}，扫描 {3} 个文件）" -f `
            $fnReport.HitFunctions, $fnReport.TotalFunctions, $unhitList.Count, $fnReport.ScannedFiles)
    $lines += ("  完整清单：$UnhitFile")
}
else {
    if (-not $NoCoverage) {
        if ($status -eq "pass") { $lines += "（未生成覆盖率报告）" }
        else { $lines += "（测试失败，跳过覆盖率展示）" }
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path $SummaryFile) | Out-Null
$lines | Set-Content -Encoding UTF8 $SummaryFile
Write-Host ""
Write-Host "== 汇总写入 $SummaryFile =="

if ($status -ne "pass") { exit 1 }

# run-integration-tests.ps1 — 集成测试入口（lua-sample 多账号并行 E2E）
#
# 与单元测试入口 run-coverage.ps1 并列。作用：
#   1. 构建 Release|Win32 的服务器 bin\jianghu.exe 与 CLI bin\cli\jianghu_cli.exe；
#   2. 用 tests\integration\config.ini 启动真实服务器（服务端命令行 --config 指定配置，
#      不再临时改写 data\config.ini），独立测试库 + 独立端口；
#   3. **一次性注册 $Workers 个账号**（并行），再**同时开跑各道剧本**
#      （scenarios\lane1..4.sce，每道一串 include 的功能片段，互不依赖）；
#      各道分别注册、分别采购、分别推进，只在共享世界里「钉给唯一一道」的
#      资源上互斥，其余全靠服务器并发推进——墙钟时间约为最长的一道而非各道之和；
#   4. 在 OpenCppCoverage 下采集 src 行覆盖，导出 Cobertura / HTML / binary；
#      所有道打的是同一个服务器进程，覆盖率天然合并，无需额外聚合；
#   5. 在控制台输出各道结果、行覆盖率与「未命中函数」清单；清单过长时只报数字，
#      完整清单写入 coverage_history\e2e-sample-unhit-functions.txt；
#   6. 任一道失败即整体失败：只输出各道结果与失败原因，不展示覆盖率与未命中
#      函数（避免覆盖率刷屏淹没失败信息；报告文件仍会导出，便于排查）。
#
# 依赖：MSBuild（VS）、OpenCppCoverage、本地 MySQL（服务 "MySQLa"，127.0.0.1:3306）。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -SkipBuild -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -Workers 2 -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -CliExe C:\tmp\jhcli.exe
#
# 说明：
#   - 使用独立测试库（tests\integration\config.ini 的 game_e2e_sample），不污染业务库；
#     每个账号用唯一角色名，因此无需清库。环境提供 mysql.exe 时可用 -ResetDb 强制重建。
#   - 覆盖率与「未命中函数」都是近似口径，详见 docs\e2e_sample_test.md。
#   - 各道的变量/报告/日志在 %TEMP%\e2e_lanes\（lane<N>.vars/.report/.log）。

param(
    # CLI 可执行文件；默认 bin\cli\jianghu_cli.exe
    [string]$CliExe = "",
    # 跳过 MSBuild 构建（直接使用现有 bin\jianghu.exe / CLI）
    [switch]$SkipBuild,
    # 跳过 OpenCppCoverage（快速调试剧本，不出覆盖率报告）
    [switch]$NoCoverage,
    # 运行前重建测试库（需要 mysql.exe 在 PATH 或 -MysqlExe）
    [switch]$ResetDb,
    # 给各道 CLI 加 --dbg：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，
    # 用于定位「客户端属性值和服务端对不上」这类问题
    [switch]$Dbg,
    [string]$MysqlExe = "mysql",
    # 单个 wait/assert 默认超时（毫秒）
    [int]$StepTimeout = 20000,
    # 等待服务器端口就绪的超时（秒）。首次在 OpenCppCoverage 下加载 PDB 可能较慢
    [int]$ServerReadyTimeoutSec = 180,
    # 长时间无输出时的心跳间隔（秒）
    [int]$HeartbeatSec = 10,
    # 控制台最多逐条打印多少个未命中函数；超出则只报数字
    [int]$UnhitListLimit = 60,
    # 并行的账号（道）数。剧本按功能域拆成 4 条互不干扰的道，每道一个账号，
    # 同时注册、同时开跑；调小会缩短并行度并丢掉未启用道的覆盖。
    [int]$Workers = 4
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
$OccExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"

# --- 道（lane）定义 -----------------------------------------------------------
# 每条道 = 一个独立账号 + 一串 include 串起来的功能片段，彼此互不依赖：
# 各道分别注册、分别采购、分别推进，靠共享世界里「钉给唯一一道」的资源隔离。
# 改这里即可增删道（Scripts 对应 scenarios\ 下的 lane*.sce）。
$Lanes = @(
    @{ Id = 1; Desc = "山脚基础+房屋"; Script = "lane_foot.sce" },
    @{ Id = 2; Desc = "武学链"; Script = "lane_skill.sce" },
    @{ Id = 3; Desc = "山顶战斗尾"; Script = "lane_combat.sce" },
    @{ Id = 4; Desc = "社交生活"; Script = "lane_social.sce" }
)

if ($Workers -lt 1) { $Workers = 1 }
if ($Workers -gt $Lanes.Count) { $Workers = $Lanes.Count }
$ActiveLanes = @($Lanes | Select-Object -First $Workers)
if ($Workers -lt $Lanes.Count) {
    Write-Warning "Workers=$Workers 只跑 $($ActiveLanes.Count)/$($Lanes.Count) 条道，未启用的道不会执行，覆盖率会低于完整运行。"
}

# 每道独立的变量文件 / 剧本报告 / 输出日志（变量文件承载本道自己的 acct）
$WorkDir = Join-Path $env:TEMP "e2e_lanes"
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
foreach ($lane in $ActiveLanes) {
    $lane.VarsFile = Join-Path $WorkDir ("lane{0}.vars" -f $lane.Id)
    $lane.ReportFile = Join-Path $WorkDir ("lane{0}.report" -f $lane.Id)
    $lane.LogFile = Join-Path $WorkDir ("lane{0}.log" -f $lane.Id)
}

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

# --- 生成各道的 vars（唯一角色名 + 邮箱） --------------------------------------
# 角色名长度限制为 2~10 字节（LoginRegister::check_legal_name / make_body）：
# "e2e"(3) + 6 位十六进制(6) + 1 位道序号(1) = 10 字节。同轮内各道靠末位道序号
# 互不重名，跨轮靠 6 位时间片段避免与历史账号撞名（重名会注册失败）。
$RunHex = '{0:x6}' -f ([DateTime]::UtcNow.Ticks -band 0xFFFFFF)
foreach ($lane in $ActiveLanes) {
    $lane.Name = "e2e$RunHex$($lane.Id)"
    $lane.Email = "e2e$RunHex$($lane.Id)@example.com"
    $lane.RegReport = Join-Path $WorkDir ("lane{0}.register.report" -f $lane.Id)
    $lane.Status = "fail"
    $lane.Steps = 0
    $lane.Reason = ""
    $lane.Result = $null
    "name=$($lane.Name)`npass=e2e1234`nemail=$($lane.Email)" |
        Set-Content -Encoding ASCII $lane.VarsFile
}

# ===========================================================================
# 运行
# ===========================================================================

$serverProc = $null
$occRun = $null
$status = "fail"
$failReason = ""

# 事件 action 跑在独立 runspace：只把行塞进本道自己的共享队列，不直接刷屏——
# 多道并行时逐行打印会互相穿插淹没控制台。进度改由各道心跳汇报，完整输出落盘
# 到各道的 .log，任一道失败时再把它的尾部打出来。退出码与剧本报告（--report）
# 都不依赖这条输出流，故即使事件没及时回来，判定结果依然可靠。
$script:EmitLaneLine = {
    $line = $EventArgs.Data
    if ($null -eq $line -or $line -eq '') { return }
    $md = $Event.MessageData
    $md.Queue.Enqueue($line)
    $md.State.Last = $line
    $md.State.Count = [int]$md.State.Count + 1
}

# 并行跑一批无头剧本：每项开一个 CLI 进程同时在跑，轮询给每项打心跳，
# 全部退出后收集输出、写日志、解析 PASS 步数。
# 输入每项：Tag / Arguments[] / LogFile；返回项附带 ExitCode / Output / Steps。
function Invoke-ParallelBatch {
    param(
        [Parameter(Mandatory)] [hashtable[]]$Items,
        [Parameter(Mandatory)] [string]$Phase
    )

    $entries = New-Object System.Collections.ArrayList
    $completed = $false
    try {
        foreach ($it in $Items) {
            $psi = New-HiddenStartInfo -Exe $CliExe -WorkingDirectory $RepoRoot -Arguments $it.Arguments
            $psi.RedirectStandardOutput = $true
            $psi.RedirectStandardError = $true
            $p = New-Object System.Diagnostics.Process
            $p.StartInfo = $psi
            [void]$p.Start()

            $state = [hashtable]::Synchronized(@{ Last = ""; Count = 0 })
            $shared = @{
                Queue = [System.Collections.Concurrent.ConcurrentQueue[string]]::new()
                State = $state
            }
            $outName = "e2e-$Phase-$($it.Tag)-out"
            $errName = "e2e-$Phase-$($it.Tag)-err"
            $ev = @()
            $ev += Register-ObjectEvent -InputObject $p -EventName OutputDataReceived `
                -SourceIdentifier $outName -MessageData $shared -Action $script:EmitLaneLine
            $ev += Register-ObjectEvent -InputObject $p -EventName ErrorDataReceived `
                -SourceIdentifier $errName -MessageData $shared -Action $script:EmitLaneLine
            $p.BeginOutputReadLine()
            $p.BeginErrorReadLine()

            [void]$entries.Add([pscustomobject]@{
                    Tag      = $it.Tag
                    Proc     = $p
                    Queue    = $shared.Queue
                    State    = $state
                    OutName  = $outName
                    ErrName  = $errName
                    Events   = $ev
                    LogFile  = $it.LogFile
                    Started  = Get-Date
                    ExitCode = -1
                    Output   = @()
                    Steps    = 0
                })
        }

        # 轮询心跳直到全部退出；每 HeartbeatSec 给每道报一次状态
        $nextBeat = (Get-Date).AddSeconds($HeartbeatSec)
        while ($true) {
            $pending = @($entries | Where-Object { -not $_.Proc.HasExited })
            if ($pending.Count -eq 0) { break }

            $now = Get-Date
            if ($now -ge $nextBeat) {
                $nextBeat = $now.AddSeconds($HeartbeatSec)
                foreach ($e in $entries) {
                    if ($e.Proc.HasExited) { continue }
                    $last = [string]$e.State.Last
                    if ($last -eq '') { $last = "（尚无输出）" }
                    if ($last.Length -gt 44) { $last = $last.Substring(0, 44) + "..." }
                    Write-Host ("   [{0}] {1,3}s 收 {2,4} 行 | {3}" -f `
                            $e.Tag, [int]($now - $e.Started).TotalSeconds, [int]$e.State.Count, $last)
                }
            }
            Start-Sleep -Milliseconds 300
        }

        # 等异步读取收尾，保证输出队列完整；再取退出码、落日志、解析步数
        foreach ($e in $entries) {
            $e.Proc.WaitForExit()
            $e.ExitCode = $e.Proc.ExitCode

            $out = @()
            $line = ""
            while ($e.Queue.TryDequeue([ref]$line)) { $out += $line }
            $e.Output = $out
            $out | Set-Content -Encoding UTF8 $e.LogFile

            $m = [regex]::Match(($out -join "`n"), '\[scenario\] PASS:\s*(\d+)\s*steps')
            if ($m.Success) { $e.Steps = [int]$m.Groups[1].Value }
        }
        $completed = $true
    }
    finally {
        foreach ($e in $entries) {
            foreach ($name in @($e.OutName, $e.ErrName)) {
                Unregister-Event -SourceIdentifier $name -ErrorAction SilentlyContinue
            }
            foreach ($j in $e.Events) { Remove-Job -Job $j -Force -ErrorAction SilentlyContinue }
            if (-not $completed) {
                # 异常路径上不留孤儿 CLI，否则它会占着测试角色继续跑
                try { if (-not $e.Proc.HasExited) { $e.Proc.Kill() } } catch { }
            }
            $e.Proc.Dispose()
        }
    }
    return $entries
}

# 打印某道输出的尾部（失败时用，定位失败步骤）
function Show-LaneTail($result, [int]$lines) {
    if ($null -eq $result) { return }
    $tail = @($result.Output | Select-Object -Last $lines)
    foreach ($l in $tail) { Write-Host "        $l" }
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

    # --- 阶段 1：一次性注册 N 个账号（并行） ---------------------------------
    Write-Host "-- 阶段 1：并行注册 $($ActiveLanes.Count) 个账号"
    $regItems = @()
    foreach ($lane in $ActiveLanes) {
        $regItems += @{
            Tag       = "L$($lane.Id)"
            Arguments = @("--config", $CliConf,
                "--script", (Join-Path $ScenarioDir "register.sce"),
                "--vars", $lane.VarsFile,
                "--step-timeout", "$StepTimeout",
                "--report", $lane.RegReport)
            LogFile   = Join-Path $WorkDir ("lane{0}.register.log" -f $lane.Id)
        }
    }
    $regResults = Invoke-ParallelBatch -Items $regItems -Phase "reg"

    $regBad = @($regResults | Where-Object { $_.ExitCode -ne 0 })
    foreach ($r in $regBad) {
        Write-Warning "账号注册失败：$($r.Tag)"
        Show-LaneTail $r 30
        Write-Host "        完整输出：$($r.LogFile)"
    }
    if ($regBad.Count -gt 0) { throw "$($regBad.Count) 个账号注册失败" }
    foreach ($lane in $ActiveLanes) {
        if (-not (Test-Path $lane.VarsFile)) { throw "注册后缺少变量文件：$($lane.VarsFile)" }
    }
    Write-Host "-- $($ActiveLanes.Count) 个账号注册完成"

    # --- 阶段 2：并行开跑各道 -------------------------------------------------
    Write-Host "-- 阶段 2：并行开跑 $($ActiveLanes.Count) 条道"
    foreach ($lane in $ActiveLanes) {
        Write-Host ("     L{0}  {1,-14} {2}" -f $lane.Id, $lane.Desc, $lane.Script)
    }
    $laneItems = @()
    foreach ($lane in $ActiveLanes) {
        $laneArgs = @("--config", $CliConf,
            "--script", (Join-Path $ScenarioDir $lane.Script),
            "--vars", $lane.VarsFile,
            "--step-timeout", "$StepTimeout",
            "--report", $lane.ReportFile)
        if ($Dbg) { $laneArgs += "--dbg" }
        $laneItems += @{
            Tag       = "L$($lane.Id)"
            Arguments = $laneArgs
            LogFile   = $lane.LogFile
        }
    }
    $laneResults = Invoke-ParallelBatch -Items $laneItems -Phase "lane"

    # 汇总：以剧本报告（--report 的 status=）为准，CLI 退出码兜底
    foreach ($r in $laneResults) {
        $id = [int]($r.Tag -replace '^L', '')
        $lane = @($ActiveLanes | Where-Object { $_.Id -eq $id }) | Select-Object -First 1
        if ($null -eq $lane) { continue }
        $lane.Result = $r
        $lane.Steps = $r.Steps

        $rep = @{}
        if (Test-Path $lane.ReportFile) {
            foreach ($l in Get-Content -Encoding UTF8 $lane.ReportFile) {
                if ($l -match '^([^=]+)=(.*)$') { $rep[$Matches[1]] = $Matches[2] }
            }
        }

        if ($r.ExitCode -eq 0 -and $rep['status'] -eq 'pass') {
            $lane.Status = 'pass'
        }
        else {
            $lane.Status = 'fail'
            $reason = $rep['reason']
            if (-not $reason) { $reason = "CLI 退出码 $($r.ExitCode)" }
            if ($rep['line']) { $reason = "$reason（剧本行 $($rep['line'])）" }
            $lane.Reason = $reason
        }
    }

    $badLanes = @($ActiveLanes | Where-Object { $_.Status -ne 'pass' })
    foreach ($lane in $badLanes) {
        Write-Warning "L$($lane.Id) $($lane.Desc) 失败：$($lane.Reason)"
        Show-LaneTail $lane.Result 30
        Write-Host "        完整输出：$($lane.LogFile)"
    }

    if ($badLanes.Count -eq 0) {
        $status = "pass"
        Write-Host "-- $($ActiveLanes.Count) 条道全部通过"
    }
    else {
        throw "$($badLanes.Count)/$($ActiveLanes.Count) 条道失败"
    }
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
Write-Host "并行道数    : $($ActiveLanes.Count)（$(($ActiveLanes | ForEach-Object { $_.Name }) -join ' ')）"
foreach ($lane in $ActiveLanes) {
    $mark = if ($lane.Status -eq 'pass') { "PASS" } else { "FAIL" }
    $steps = if ($lane.Steps -gt 0) { "$($lane.Steps) steps" } else { "-" }
    Write-Host ("  L{0} {1,-14} {2}  ({3})" -f $lane.Id, $lane.Desc, $mark, $steps)
    if ($lane.Status -ne 'pass' -and $lane.Reason) {
        Write-Host ("      原因：{0}" -f $lane.Reason)
    }
}
if ($status -ne "pass" -and -not $NoCoverage -and (Test-Path $ReportDir)) {
    Write-Host ""
    Write-Host "（集成测试失败：跳过覆盖率与未命中函数展示；报告仍在 $ReportDir）"
}

$lines += "lua-sample E2E：$status"
if ($failReason) { $lines += "reason=$failReason" }
$lines += "workers=$($ActiveLanes.Count)"
foreach ($lane in $ActiveLanes) {
    $lines += ("L{0}={1} name={2} steps={3}{4}" -f $lane.Id, $lane.Status, $lane.Name,
        $lane.Steps, $(if ($lane.Reason) { " reason=$($lane.Reason)" } else { "" }))
}
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

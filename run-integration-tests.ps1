# run-integration-tests.ps1 — 集成测试入口（lua-sample 多账号并行 E2E）
#
# 与单元测试入口 run-coverage.ps1 并列。作用：
#   1. 构建 Release|Win32 的服务器 bin\jianghu.exe 与 CLI bin\cli\jianghu_cli.exe；
#   2. 用 tests\integration\config.ini 启动真实服务器（服务端命令行 --config 指定配置，
#      不再临时改写 data\config.ini），独立测试库 + 独立端口；
#   3. **一次性注册 $Workers 个账号**（并行），再**同时开跑各道剧本**
#      （scenarios\lane_*.sce，每道一串 include 的功能片段，彼此互不依赖；
#      例外是互动道：L5/L6 配对，注册阶段互相注入对方角色名与账号
#      （vars 的 peer/peer_acct 键）会合后做玩家对玩家的互动；L7 观战道
#      单向依附 L5（本道 vars 收 peer 键、L5 收 spectator 键），为 L5/L6
#      的 PK 段提供房外观战/加入的第三方）；
#      各道分别注册、分别采购、分别推进，只在共享世界里「钉给唯一一道」的
#      资源上互斥，其余全靠服务器并发推进——墙钟时间约为最长的一道而非各道之和；
#   4. 在 OpenCppCoverage 下采集 src 行覆盖，导出 Cobertura / HTML / binary；
#      所有道打的是同一个服务器进程，覆盖率天然合并，无需额外聚合；
#      统计时按叶子文件名跳过 LuaSyntaxChecker.cpp 与 CmdWizard*.cpp（巫师指令），
#      口径对齐 run-coverage.ps1 的 $excludedFiles。
#   5. 在控制台输出各道结果、行覆盖率与「未命中函数」清单；清单过长时只报数字，
#      完整清单写入 coverage_history\e2e-sample-unhit-functions.txt；
#   6. 任一道失败即整体失败：只输出各道结果与失败原因，不展示覆盖率与未命中
#      函数（避免覆盖率刷屏淹没失败信息；报告文件仍会导出，便于排查）。
#   7. 崩溃检测：启动前先把**上一轮遗留**的旧 dump 归档到仓库根 crash_dumps\
#      （避免在仓库根无限堆积），再快照仓库根 *.dmp，收尾停服后 diff——服务器
#      未处理异常（关服路径崩溃）会生成新 dump，判定为整体失败。**本轮新产生的
#      dump 原地留在仓库根**并打印完整路径，绝不挪走：挪到 %TEMP% 会让人看不到
#      崩溃证据、误以为崩溃已修好（「剧本全绿、服务器其实崩了」同理要防）。
#   8. 耗时监控：每次 PASS 运行把「剧本执行总耗时」（注册 + 各道并行，不含构建与
#      覆盖率导出）连同各道耗时/步数/场景清单追加到
#      coverage_history\e2e-timing-history.jsonl（保留最近 50 条），并与前次同类
#      通过运行（同 workers、同覆盖率开关）对比；增量超过 -DurationRegressionSec
#      （默认 10 秒）时 diff 各道 include 场景清单归因是否新增场景导致，并按
#      关键路径估算「移入现有道 / 新增一条道」的拆分收益。
#
# 依赖：MSBuild（VS）、OpenCppCoverage。数据库为 SQLite（tests\integration\e2e.db），
#        由服务端启动时自动建库建表，无需任何外部数据库服务。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -SkipBuild -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -Workers 2 -NoCoverage
#   powershell -ExecutionPolicy Bypass -File run-integration-tests.ps1 -CliExe C:\tmp\jhcli.exe
#
# 说明：
#   - 使用独立 SQLite 文件库（tests\integration\config.ini 的 e2e.db），不污染业务库；
#     每个账号用唯一角色名，因此无需清库。需要全新库时用 -ResetDb 删除库文件，
#     服务端下次启动自动重建。
#   - 覆盖率与「未命中函数」都是近似口径，详见 docs\design\integration-test.md §5.3。
#   - 各道的变量/报告/日志在 %TEMP%\e2e_lanes\（lane<N>.vars/.report/.log）。
#   - 进程管理只认「监听测试端口的 jianghu」：启动前只回收上次残留的测试服务器，
#     收尾只停本脚本起的那台，不会按进程名误杀你在其他端口（如 6600）跑的服务器；
#     但也因此不要把功能测试服务器开在测试端口（tests\integration\config.ini 的 port）上。

param(
    # CLI 可执行文件；默认 bin\cli\jianghu_cli.exe
    [string]$CliExe = "",
    # 跳过 MSBuild 构建（直接使用现有 bin\jianghu.exe / CLI）
    [switch]$SkipBuild,
    # 跳过 OpenCppCoverage（快速调试剧本，不出覆盖率报告）
    [switch]$NoCoverage,
    # 运行前重建测试库：删除 SQLite 库文件（服务端启动自动重建）
    [switch]$ResetDb,
    # 给各道 CLI 加 --dbg：把服务端报文（RTNINFO/STATE 等）打进 lane 日志，
    # 用于定位「客户端属性值和服务端对不上」这类问题
    [switch]$Dbg,
    # 单个 wait/assert 默认超时（毫秒）
    [int]$StepTimeout = 20000,
    # 等待服务器端口就绪的超时（秒）。首次在 OpenCppCoverage 下加载 PDB 可能较慢
    [int]$ServerReadyTimeoutSec = 180,
    # 长时间无输出时的心跳间隔（秒）
    [int]$HeartbeatSec = 10,
    # 控制台最多逐条打印多少个未命中函数；超出则只报数字
    [int]$UnhitListLimit = 60,
    # 并行的账号（道）数。剧本按功能域拆成 7 条道（L1~L4 互不干扰 + L5/L6
    # 配对互动道 + L7 观战道单向依附 L5；配对与依附关系都必须同时启用），
    # 每道一个账号，同时注册、同时开跑；
    # 调小会缩短并行度并丢掉未启用道的覆盖。
    [int]$Workers = 7,
    # 耗时监控：本次剧本执行总耗时较前次同类通过运行增加超过该秒数时，
    # 归因是否新增场景导致，并探索拆分（移道 / 新增道）的可能性
    [int]$DurationRegressionSec = 10
)

$ErrorActionPreference = "Stop"

# 起点计时（耗时监控用，文首第 8 条）；各阶段秒数在对应阶段赋值，未到达即为 $null
$tScriptStart = Get-Date
$tBuildSec = $null
$tServerReadySec = $null
$tRegSec = $null
$tLaneSec = $null
$tExportSec = $null

# --- 路径 -------------------------------------------------------------------
$RepoRoot = $PSScriptRoot
$IntDir = Join-Path $RepoRoot "tests\integration"
$ConfPath = Join-Path $IntDir "config.ini"
$ScenarioDir = Join-Path $IntDir "scenarios"
$CliConf = Join-Path $IntDir "cli.conf"
$ServerProj = Join-Path $RepoRoot "jianghu.vcxproj"
$CliProj = Join-Path $RepoRoot "cli\jianghu_cli.vcxproj"
# 集成测试用**专属**服务器 exe（bin\e2e\jianghu.exe）：与正常使用
# bin\jianghu.exe（如功能测试服跑在 6600）互不抢同一个文件锁，可并存。
$E2EDir = Join-Path $RepoRoot "bin\e2e"
$ServerExe = Join-Path $E2EDir "jianghu.exe"
$ReportDir = Join-Path $RepoRoot "coverage_report_e2e_sample"
$CovBin = Join-Path $RepoRoot "coverage_history\e2e_sample.cov"
$SummaryFile = Join-Path $RepoRoot "coverage_history\summary-e2e-sample.txt"
$UnhitFile = Join-Path $RepoRoot "coverage_history\e2e-sample-unhit-functions.txt"
# 耗时历史：每次 PASS 追加一条 JSON（各阶段耗时 + 各道场景清单），供与前次对比归因
$TimingHistoryFile = Join-Path $RepoRoot "coverage_history\e2e-timing-history.jsonl"
$TimingKeepCount = 50
$OccExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe"

# --- 道（lane）定义 -----------------------------------------------------------
# 每条道 = 一个独立账号 + 一串 include 串起来的功能片段，彼此互不依赖：
# 各道分别注册、分别采购、分别推进，靠共享世界里「钉给唯一一道」的资源隔离。
# 改这里即可增删道（Scripts 对应 scenarios\ 下的 lane*.sce）。
$Lanes = @(
    @{ Id = 1; Desc = "山脚基础+房屋"; Script = "lane_foot.sce" },
    @{ Id = 2; Desc = "武学链"; Script = "lane_skill.sce" },
    @{ Id = 3; Desc = "山顶战斗尾"; Script = "lane_combat.sce" },
    @{ Id = 4; Desc = "社交生活"; Script = "lane_social.sce" },
    # L5/L6 是配对互动道（PeerId 互相指认）：注册阶段把对方角色名写进对方
    # vars（peer 键），剧本以 $peer 会合后做玩家互动（组队/私聊/赠送/交易等）。
    # L5 的 WitnessId=7 指向观战道：L5 vars 收 spectator 键（对方角色名），
    # 注册后同样互注 spectator_acct——pk/fight 段跨房 tell 开战信号用。
    @{ Id = 5; Desc = "互动A"; Script = "lane_interaction_a.sce"; PeerId = 6; WitnessId = 7 },
    @{ Id = 6; Desc = "互动B"; Script = "lane_interaction_b.sce"; PeerId = 5 },
    # L7 观战道（PeerId=5 单向依附 L5）：L5/L6 打 PK 与死亡链时它待在
    # 会合房间房外，做比试房观战（guard）与战斗房加入（join）。
    @{ Id = 7; Desc = "观战"; Script = "lane_spectate.sce"; PeerId = 5 }
)

if ($Workers -lt 1) { $Workers = 1 }
if ($Workers -gt $Lanes.Count) { $Workers = $Lanes.Count }
$ActiveLanes = @($Lanes | Select-Object -First $Workers)
if ($Workers -lt $Lanes.Count) {
    Write-Warning "Workers=$Workers 只跑 $($ActiveLanes.Count)/$($Lanes.Count) 条道，未启用的道不会执行，覆盖率会低于完整运行。"
}

# 配对互动道必须成对启用：L5/L6 互为 peer，只启用一半会因找不到对方角色名
# 而在会合片段超时，这里提前给出明确报错。同理，L5 依赖观战道 L7（spectator
# 注入 + PK 段信号）、L7 依赖 L5（peer 注入），任何一半缺位都在此拦下。
foreach ($lane in $ActiveLanes) {
    if ($lane.PeerId -and -not (@($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1)) {
        throw "道 L$($lane.Id)（$($lane.Script)）依赖配对道 L$($lane.PeerId)（peer 互注），但 L$($lane.PeerId) 未启用：请把 -Workers 调到 $(@($Lanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1 | ForEach-Object { $_.Id }) 及以上。"
    }
    if ($lane.WitnessId -and -not (@($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1)) {
        throw "道 L$($lane.Id)（$($lane.Script)）依赖观战道 L$($lane.WitnessId)（spectator 注入与 PK 段信号），但 L$($lane.WitnessId) 未启用：请把 -Workers 调到 $($lane.WitnessId) 及以上。"
    }
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

# 统计覆盖率时跳过的文件（叶子名，支持通配符）——口径对齐 run-coverage.ps1 的
# $excludedFiles：E2E 走不到也不该走到的代码不进分母/未命中清单。
$script:CoverageSkipPatterns = @(
    "LuaSyntaxChecker.cpp",   # Lua 语法检查器：剧本不触发
    "CmdWizard*.cpp"          # 巫师指令：剧本不覆盖
)

function Test-CoverageSkippedFile {
    param([string]$FileName)
    $leaf = Split-Path $FileName -Leaf
    foreach ($pat in $script:CoverageSkipPatterns) {
        if ($leaf -like $pat) { return $true }
    }
    return $false
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
        if (Test-CoverageSkippedFile $rel) { continue }

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

# 定位「本次测试的服务器」——**只按测试端口认进程，绝不按进程名全服清场**。
# 有人会用同一个 bin\jianghu.exe 在别的端口（data\config.ini 默认 6600）做功能测试，
# 按进程名 Get-Process jianghu 杀会把那台服务器误杀。
# -NoCoverage 下服务器由本脚本直接拉起，句柄最可靠；覆盖率模式下服务器是
# OpenCppCoverage 的子进程、本脚本没有句柄，改用「监听测试端口的 jianghu 进程」定位。
function Get-TestServerProcess {
    param([int]$Port)

    if ($null -ne $serverProc -and -not $serverProc.HasExited) { return $serverProc }

    $conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $conn) { return $null }
    $p = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
    if ($null -ne $p -and $p.ProcessName -eq 'jianghu') { return $p }
    return $null
}

function Stop-ServerGraceful {
    param([int]$Port)

    $p = Get-TestServerProcess -Port $Port
    if ($null -eq $p) { return }
    $serverPid = $p.Id
    try { [void]$p.CloseMainWindow() } catch { }
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        # 按 pid 等，不能按进程名等：按名等时别的 jianghu 还在，
        # 会白等满 30 秒、再把它和测试服务器一起强杀
        if (-not (Get-Process -Id $serverPid -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Milliseconds 500
    }
    Get-Process -Id $serverPid -ErrorAction SilentlyContinue | Stop-Process -Force
}

# 启动前清场：只回收「上一次运行残留、仍监听测试端口」的测试服务器。
# 端口被别的程序占用时直接报错，而不是连它一起杀（否则 Wait-Port 会误判就绪，
# 后续 CLI 全连到错误的进程上）。
function Stop-OrphanTestServer {
    param([int]$Port)

    $owningPids = @(Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty OwningProcess -Unique)
    foreach ($procId in $owningPids) {
        $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
        if ($null -eq $p) { continue }
        if ($p.ProcessName -ne 'jianghu') {
            throw "端口 $Port 被其他进程占用：$($p.ProcessName)（pid $procId），请先释放该端口（本脚本只回收自己的测试服务器）。"
        }
        Write-Host "-- 回收上次运行残留的测试服务器（pid $procId，端口 $Port）"
        Stop-Process -Id $procId -Force
    }
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
$DbType = Get-IniValue $ConfPath "database" "type"
if ($DbType -ne "sqlite") { throw "$ConfPath 的 [database] type 必须为 sqlite（E2E 已不依赖外部数据库服务），当前：'$DbType'" }
$DbPath = Get-IniValue $ConfPath "database" "path"
if ($DbPath -eq "") { throw "无法从 $ConfPath 解析 [database] path" }
# config 里的 path 相对仓库根（服务器以仓库根为工作目录启动）；脚本侧拼成绝对路径
if (-not [System.IO.Path]::IsPathRooted($DbPath)) { $DbPath = Join-Path $RepoRoot ($DbPath -replace "/", "\") }
if ($CliExe -eq "") { $CliExe = Join-Path $RepoRoot "bin\cli\jianghu_cli.exe" }

Write-Host "== lua-sample 集成测试（单人主线 E2E）=="
Write-Host "repo     : $RepoRoot"
Write-Host "config   : $ConfPath"
Write-Host "port     : $Port   db: sqlite $DbPath"

# --- 构建 -------------------------------------------------------------------
if (-not $SkipBuild) {
    $msb = Get-MSBuildPath
    Write-Host "-- 构建服务器与 CLI（Release|Win32）"
    $t0 = Get-Date
    # CLI 正常输出到 bin\cli\；服务器单独输出到 bin\e2e\（见上 $E2EDir 注释），
    # 使集成测试写自己的 exe，不与正在运行的 bin\jianghu.exe 抢文件锁（LNK1104）。
    # 注意用双写结尾反斜杠：既满足 OutDir 必须以 \ 结尾，又避开 PowerShell 向
    # 原生命令传「单个结尾反斜杠」被吃掉的坑。
    & $msb $CliProj /p:Configuration=Release /p:Platform=Win32 /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "构建失败：$CliProj" }
    New-Item -ItemType Directory -Force -Path $E2EDir | Out-Null
    & $msb $ServerProj /p:Configuration=Release /p:Platform=Win32 "/p:OutDir=$E2EDir\\" /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "构建失败：$ServerProj" }
    # 虽然 E2E 数据库走 SQLite（sqlite3 静态编入），但 jianghu.exe 仍隐式链接
    # libmysql.lib（非 delay-load），进程启动时加载器就要求 libmySQL.dll 在场——
    # 故 bin\*.dll 仍随 exe 带到 bin\e2e\。只在目标缺失时复制：bin\e2e\ 可能残留
    # 上一次未退净的服务器（占着 DLL，脚本稍后在服务启动阶段按测试端口定向回收），
    # 此时 DLL 已在、无需强写。
    foreach ($dll in Get-ChildItem (Join-Path $RepoRoot "bin\*.dll") -ErrorAction SilentlyContinue) {
        $dst = Join-Path $E2EDir $dll.Name
        if (-not (Test-Path $dst)) {
            Copy-Item -Destination $dst -ErrorAction SilentlyContinue
        }
    }
    $tBuildSec = [int]((Get-Date) - $t0).TotalSeconds
}

foreach ($p in @($ServerExe, $CliExe)) {
    if (-not (Test-Path $p)) { throw "缺少文件：$p（可去掉 -SkipBuild 重新构建）" }
}
if (-not $NoCoverage -and -not (Test-Path $OccExe)) {
    throw "找不到 OpenCppCoverage：$OccExe（可用 -NoCoverage 跳过）"
}

# --- 数据库（可选重建） -----------------------------------------------------
# SQLite 的「重建库」= 删库文件（连带 WAL/SHM 附属文件），服务端启动时
# sqlite3_open_v2 自动重建、prepare_tables 自动建表播种，无需任何外部工具。
if ($ResetDb) {
    Write-Host "-- 重建测试库 $DbPath"
    foreach ($f in @($DbPath, "$DbPath-wal", "$DbPath-shm")) {
        if (Test-Path $f) { Remove-Item -Force $f }
    }
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

# 配对互动道 peer 注入：把对方角色名写进本道 vars（peer 键），剧本以 $peer 引用。
# 角色名在此阶段已全部确定（上一循环），注册阶段 CLI 会原样保留 vars 里已有的键
# （CScenarioRunner 的 load/save 只增 acct 等产出键，不动输入键），故无需改 CLI。
foreach ($lane in $ActiveLanes) {
    if (-not $lane.PeerId) { continue }
    $peerLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1
    Add-Content -Encoding ASCII $lane.VarsFile "peer=$($peerLane.Name)"
}

# 观战道 spectator 注入：把 L7 角色名写进依赖道（L5）的 vars（spectator 键），
# 与 peer 同机制同阶段；剧本以 $spectator / $spectator_acct 做跨房开战信号。
foreach ($lane in $ActiveLanes) {
    if (-not $lane.WitnessId) { continue }
    $witLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1
    Add-Content -Encoding ASCII $lane.VarsFile "spectator=$($witLane.Name)"
}

# ===========================================================================
# 运行
# ===========================================================================

$serverProc = $null
$occRun = $null
$status = "fail"
$failReason = ""
# 崩溃检测基线：本次运行启动服务器前仓库根已有的 dump 文件集合。
# $null 表示快照没做成（服务器未启动），此时跳过崩溃检测。
$dumpsBefore = $null
# 旧 dump 归档目录（仓库根 crash_dumps\）：启动前把上一轮遗留 dump 挪进去；
# 本轮新产生的 dump 绝不挪动，原地留在仓库根供查看。
$OldDumpArchive = Join-Path $RepoRoot "crash_dumps"

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
                    DurationSec = 0
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
            # 单道耗时必须用进程真实退出时刻：本循环在全部进程退出后才执行，
            # 若在此处取 Get-Date，所有道都会记成批总时长（关键路径分析会失真）。
            try {
                $e.DurationSec = [Math]::Max(0, [int]($e.Proc.ExitTime - $e.Started).TotalSeconds)
            }
            catch {
                $e.DurationSec = [Math]::Max(0, [int]((Get-Date) - $e.Started).TotalSeconds)
            }
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

# ===========================================================================
# 耗时监控（文首第 8 条）
# ===========================================================================

# 递归展开剧本的 include，返回相对 scenarios 目录的场景文件清单（'/' 分隔、字典序）。
# 展开口径与 CLI CScenarioRunner::load_script 一致：相对路径基于包含文件所在目录。
# include 参数含 $var 时无法静态展开，按字面量记录（当前剧本无此用法）。
function Get-LaneScenarioList {
    param([Parameter(Mandatory)][string]$LaneScript)

    $root = $ScenarioDir
    $result = New-Object System.Collections.ArrayList
    $seen = @{}
    $stack = New-Object System.Collections.Stack
    $stack.Push((Join-Path $root $LaneScript))
    while ($stack.Count -gt 0) {
        $path = [string]$stack.Pop()
        if ($seen.ContainsKey($path)) { continue }
        $seen[$path] = $true

        if ($path.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            $rel = $path.Substring($root.Length).TrimStart('\', '/')
        }
        else { $rel = $path }
        [void]$result.Add(($rel -replace '\\', '/'))

        if (-not (Test-Path -LiteralPath $path)) { continue }
        $baseDir = Split-Path $path -Parent
        foreach ($line in (Get-Content -LiteralPath $path -Encoding UTF8)) {
            $t = $line.Trim()
            if ($t -eq '' -or $t.StartsWith('#')) { continue }
            $m = [regex]::Match($t, '^include\s+(\S+)')
            if (-not $m.Success) { continue }
            $inc = $m.Groups[1].Value
            if ($inc -match '^(?:[A-Za-z]:[\\/]|[\\/])') { $stack.Push($inc) }
            else { $stack.Push((Join-Path $baseDir $inc)) }
        }
    }
    return @($result | Sort-Object)
}

# 读取耗时历史（JSON Lines，一行一次 PASS 运行）；坏行跳过。
function Read-TimingHistory {
    if (-not (Test-Path -LiteralPath $TimingHistoryFile)) { return @() }
    $list = New-Object System.Collections.ArrayList
    foreach ($line in (Get-Content -LiteralPath $TimingHistoryFile -Encoding UTF8)) {
        $t = $line.Trim()
        if ($t -eq '') { continue }
        try { [void]$list.Add(($t | ConvertFrom-Json)) } catch { }
    }
    return @($list)
}

# 追加本次记录；超出 $TimingKeepCount 条时整体裁剪（只保留最近 N 条）。
function Add-TimingRecord {
    param([Parameter(Mandatory)]$Record)

    New-Item -ItemType Directory -Force -Path (Split-Path $TimingHistoryFile -Parent) | Out-Null
    Add-Content -LiteralPath $TimingHistoryFile `
        -Value (ConvertTo-Json -InputObject $Record -Compress -Depth 6) -Encoding UTF8

    $all = @(Read-TimingHistory)
    if ($all.Count -gt $TimingKeepCount) {
        $kept = @($all | Select-Object -Last $TimingKeepCount |
            ForEach-Object { ConvertTo-Json -InputObject $_ -Compress -Depth 6 })
        Set-Content -LiteralPath $TimingHistoryFile -Value $kept -Encoding UTF8
    }
}

# 与前次同类（同 workers / 覆盖率开关 / dbg / step-timeout 且 PASS）运行对比，
# 生成报告行：总耗时与阶段构成 → 各道耗时增量 → 新增场景 diff → 关键路径 → 拆分建议。
# 拆分收益按「关键路径道的增量 ≈ 新增场景成本、墙钟 = 注册 + 最长道」估算。
function Get-TimingReport {
    param(
        [Parameter(Mandatory)]$Cur,
        $Prev,
        [Parameter(Mandatory)][int]$ThresholdSec
    )

    $out = New-Object System.Collections.ArrayList
    $regressed = $false
    $causedByNew = $false

    $regCur = [int]$Cur.reg_sec
    $laneCur = [int]$Cur.lane_sec
    $curExec = [int]$Cur.exec_sec
    $buildTxt = if ($null -ne $Cur.build_sec) { "$($Cur.build_sec)s" } else { "-" }
    $readyTxt = if ($null -ne $Cur.server_ready_sec) { "$($Cur.server_ready_sec)s" } else { "-" }
    $exportTxt = if ($null -ne $Cur.export_sec) { "$($Cur.export_sec)s" } else { "-" }
    [void]$out.Add(("本次总耗时（剧本执行）: {0}s = 注册 {1}s + 各道并行 {2}s" -f $curExec, $regCur, $laneCur))
    [void]$out.Add(("  构建 {0} ｜ 起服 {1} ｜ 覆盖率导出 {2}（不计入对比）" -f $buildTxt, $readyTxt, $exportTxt))

    if ($null -eq $Prev) {
        [void]$out.Add("无同类基线（首次运行或 workers/覆盖率/dbg/step-timeout 变更），本次已记为基线。")
        return [pscustomobject]@{ Lines = @($out); Regression = $false; CausedByNewScenario = $false }
    }

    $regPrev = [int]$Prev.reg_sec
    $lanePrev = [int]$Prev.lane_sec
    $prevExec = [int]$Prev.exec_sec
    $delta = $curExec - $prevExec
    $sign = if ($delta -ge 0) { "+" } else { "" }

    [void]$out.Add(("前次总耗时（{0}，同配置）: {1}s = 注册 {2}s + 各道并行 {3}s" -f `
            $Prev.time, $prevExec, $regPrev, $lanePrev))
    [void]$out.Add(("Δ 总耗时              : {0}{1}s（阈值 {2}s）" -f $sign, $delta, $ThresholdSec))

    if ($delta -le $ThresholdSec) {
        [void]$out.Add("Δ 未超过阈值，无需归因。")
        return [pscustomobject]@{ Lines = @($out); Regression = $false; CausedByNewScenario = $false }
    }
    $regressed = $true

    # --- 各道耗时增量（按耗时降序，首个即当前关键路径） -----------------------
    $prevLanes = @{}
    foreach ($pl in @($Prev.lanes)) { $prevLanes[[int]$pl.id] = $pl }
    $crit = $null
    foreach ($l in (@($Cur.lanes) | Sort-Object { -[int]$_.sec })) {
        $p = $null
        if ($prevLanes.ContainsKey([int]$l.id)) { $p = $prevLanes[[int]$l.id] }
        $secTxt = "{0}s" -f [int]$l.sec
        if ($null -ne $p) {
            $ld = [int]$l.sec - [int]$p.sec
            $secTxt += "（{0}{1}s）" -f $(if ($ld -ge 0) { "+" } else { "" }), $ld
        }
        else { $secTxt += "（前次无此道）" }
        [void]$out.Add(("  L{0} {1,-14} {2}  steps {3}  场景 {4}" -f `
                $l.id, $l.desc, $secTxt, $l.steps, @($l.scenarios).Count))
        if ($null -eq $crit) { $crit = $l }
    }

    # --- 新增场景 diff（含注册阶段）与步数变化 --------------------------------
    $newScen = New-Object System.Collections.ArrayList
    $addedReg = @(@($Cur.reg_scenarios) | Where-Object { @($Prev.reg_scenarios) -notcontains $_ })
    if ($addedReg.Count -gt 0) {
        [void]$newScen.Add([pscustomobject]@{ Lane = $null; Files = $addedReg })
    }
    $stepChanged = New-Object System.Collections.ArrayList
    foreach ($l in @($Cur.lanes)) {
        $prevSet = @()
        $p = $null
        if ($prevLanes.ContainsKey([int]$l.id)) { $p = $prevLanes[[int]$l.id]; $prevSet = @($p.scenarios) }
        $added = @(@($l.scenarios) | Where-Object { $prevSet -notcontains $_ })
        if ($added.Count -gt 0) {
            [void]$newScen.Add([pscustomobject]@{ Lane = $l; Files = $added })
        }
        if ($null -ne $p -and [int]$l.steps -ne [int]$p.steps) {
            [void]$stepChanged.Add(("L{0} {1} steps {2}→{3}" -f $l.id, $l.desc, $p.steps, $l.steps))
        }
    }

    $regDelta = $regCur - $regPrev
    $laneDelta = $laneCur - $lanePrev
    [void]$out.Add(("Δ 构成              : 注册 {0}{1}s ｜ 各道并行墙钟 {2}{3}s" -f `
            $(if ($regDelta -ge 0) { "+" } else { "" }), $regDelta,
            $(if ($laneDelta -ge 0) { "+" } else { "" }), $laneDelta))

    if ($newScen.Count -eq 0) {
        [void]$out.Add("归因：未发现新增场景（各道场景清单与前次一致）。")
        if ($stepChanged.Count -gt 0) {
            [void]$out.Add(("  场景内容扩充：{0}" -f ($stepChanged -join '；')))
            [void]$out.Add("  → 拆分探索：把上述道内重复片段（repeat/等待密集段）继续拆到并行度有余量的道，或复核 sleep/timeout。")
        }
        else {
            [void]$out.Add("  各道步数亦无变化 → 疑为环境波动（服务器/网络/覆盖率开销），建议复跑确认。")
        }
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $false }
    }

    # --- 归因到新增场景 -------------------------------------------------------
    foreach ($ns in $newScen) {
        $where = if ($null -eq $ns.Lane) { "注册阶段" } else { "L{0} {1}" -f $ns.Lane.id, $ns.Lane.desc }
        [void]$out.Add(("归因：新增场景 → {0} + {1}" -f $where, ($ns.Files -join '、')))
    }

    # 关键路径（当前最长道）及其增量
    $critId = [int]$crit.id
    $prevCritSec = if ($prevLanes.ContainsKey($critId)) { [int]$prevLanes[$critId].sec } else { [int]$crit.sec }
    $critDelta = [int]$crit.sec - $prevCritSec
    $critSign = if ($critDelta -ge 0) { "+" } else { "" }
    [void]$out.Add(("关键路径            : L{0} {1} {2}s（前次 {3}s，{4}{5}s）" -f `
            $critId, $crit.desc, $crit.sec, $prevCritSec, $critSign, $critDelta))

    $critNew = @($newScen | Where-Object { $null -ne $_.Lane -and [int]$_.Lane.id -eq $critId })
    $regCausedGrew = ($addedReg.Count -gt 0 -and $regDelta -gt 0)
    $critCausedGrew = ($critNew.Count -gt 0 -and $critDelta -gt 0)
    $causedByNew = $critCausedGrew -or $regCausedGrew

    if ($regCausedGrew -and -not $critCausedGrew) {
        # 注册阶段新增场景且注册变慢，关键路径道本身没变 → 归因注册
        [void]$out.Add("归因：注册阶段新增场景拉长了注册墙钟（各账号并行跑同一份 register.sce，单账号变长即整体变长）。")
        [void]$out.Add("  → 拆分探索：register.sce 无法按账号拆道；若新增步骤非必需，精简注册流程，或把耗时步骤移出注册（登录后再做）。")
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $true }
    }

    if (-not $critCausedGrew) {
        if (@($stepChanged).Count -gt 0) {
            [void]$out.Add(("  关键路径无新增场景文件，但步数变化：{0}" -f (@($stepChanged) -join '；')))
        }
        [void]$out.Add("  关键路径的增长不含新增场景 → 增长主要来自其他因素（上方 Δ 构成 / 步数变化 / 环境）。")
        $laneNewOnly = @($newScen | Where-Object { $null -ne $_.Lane })
        if ($laneNewOnly.Count -gt 0) {
            [void]$out.Add("  新增场景落在非关键路径道，当前不改变墙钟；拆分它们不会缩短总耗时。")
        }
        return [pscustomobject]@{ Lines = @($out); Regression = $true; CausedByNewScenario = $false }
    }

    # --- 拆分探索（新增场景在关键路径上） -------------------------------------
    $costEst = $critDelta
    if ($costEst -le 0) { $costEst = 0 }
    $newCritSec = [int]$crit.sec - $costEst
    $others = @(@($Cur.lanes) | Where-Object { [int]$_.id -ne $critId })
    $othersMax = 0
    foreach ($o in $others) { if ([int]$o.sec -gt $othersMax) { $othersMax = [int]$o.sec } }
    $files = @($critNew | ForEach-Object { $_.Files }) -join '、'

    [void]$out.Add(("拆分探索（按关键路径增量 {0}s 近似新增场景成本，墙钟 = 注册 + 最长道）：" -f $costEst))
    if ($costEst -gt 0) {
        if ($others.Count -gt 0) {
            $target = $null
            foreach ($o in ($others | Sort-Object { [int]$_.sec })) { $target = $o; break }
            $newTotA = $regCur + [Math]::Max([Math]::Max($newCritSec, $othersMax), [int]$target.sec + $costEst)
            $saveA = $curExec - $newTotA
            [void]$out.Add(("  · 方案 A：把 {0} 从 L{1} 移到 L{2}（现 {3}s）→ 预计总耗时 {4}s（省 {5}s）" -f `
                    $files, $critId, $target.id, $target.sec, $newTotA, $saveA))
        }
        $newTotB = $regCur + [Math]::Max([Math]::Max($newCritSec, $othersMax), $costEst)
        $saveB = $curExec - $newTotB
        [void]$out.Add(("  · 方案 B：新增一条并行道（`$Lanes 加一项 + -Workers 调大）承接 {0} → 预计总耗时 {1}s（省 {2}s）" -f `
                $files, $newTotB, $saveB))
        if ($saveA -le 0 -and $saveB -le 0) {
            [void]$out.Add("  · 估算收益 ≤ 0：次长道已逼近关键路径，单纯移动不缩短墙钟——应拆分关键路径道内的既有片段。")
        }
    }
    else {
        [void]$out.Add("  关键路径较前次无增量（其他道/注册贡献了 Δ），先核对上方 Δ 构成再决定是否拆分。")
    }
    [void]$out.Add("  ⚠ 拆分前核对：新增场景是否依赖钉给某道的稀缺资源（集成测试指令 §8.4）、/t 目标（§8.5）。")

    return [pscustomobject]@{ Lines = @($out); Regression = $regressed; CausedByNewScenario = $causedByNew }
}

try {
    # --- 启动服务器（--config 指向集成测试配置） -----------------------------
    # 只回收上次运行残留的测试服务器（认端口不认进程名，不碰其他端口上的 jianghu）
    Stop-OrphanTestServer -Port $Port
    Start-Sleep -Seconds 1

    # 崩溃检测：服务器的未处理异常过滤器（CCreateDump）会往工作目录（仓库根）写
    # "[时间]dumpfile.dmp"。先把上一轮遗留的旧 dump 归档到仓库根 crash_dumps\
    # （在仓库里，看得见；也避免在仓库根无限堆积），再快照当前集合——收尾停服后
    # diff，新增的即本次崩溃证据。
    $oldDumps = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue)
    if ($oldDumps.Count -gt 0) {
        New-Item -ItemType Directory -Force -Path $OldDumpArchive | Out-Null
        foreach ($d in $oldDumps) {
            try {
                Move-Item -LiteralPath $d.FullName -Destination (Join-Path $OldDumpArchive $d.Name) -Force
            }
            catch {
                Write-Warning "归档旧 dump 失败（保留原地）：$($d.FullName) — $($_.Exception.Message)"
            }
        }
    }
    $dumpsBefore = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue |
            ForEach-Object { $_.FullName })

    $tServerStart = Get-Date
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
    $tServerReadySec = [int]((Get-Date) - $tServerStart).TotalSeconds
    Write-Host "-- 服务器就绪"

    # --- 阶段 1：一次性注册 N 个账号（并行） ---------------------------------
    Write-Host "-- 阶段 1：并行注册 $($ActiveLanes.Count) 个账号"
    $t0 = Get-Date
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
    $tRegSec = [int]((Get-Date) - $t0).TotalSeconds

    # 注册完成后回读各道 acct，向配对两道互注 peer_acct（对方数字账号 ID）。
    # /t 只收 ID：玩家=数字账号 ID（全服有效、跨房间），NPC/物品=英文 id
    # （当前环境）；显示名允许重名、不作目标——$peer 仅保留给文本断言，
    # 剧本定向对方一律 /t $peer_acct。
    foreach ($lane in $ActiveLanes) {
        if (-not $lane.PeerId) { continue }
        $peerLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.PeerId }) | Select-Object -First 1
        $peerAcct = $null
        if (Test-Path $peerLane.VarsFile) {
            $m = Select-String -Path $peerLane.VarsFile -Pattern '^acct=(.+)$' |
                Select-Object -Last 1
            if ($m) { $peerAcct = $m.Matches[0].Groups[1].Value.Trim() }
        }
        if (-not $peerAcct) {
            throw "注册后未取到 L$($peerLane.Id) 的 acct（$($peerLane.VarsFile)），无法注入 peer_acct"
        }
        Add-Content -Encoding ASCII $lane.VarsFile "peer_acct=$peerAcct"
    }

    # 观战道 spectator_acct 注入（同上）：L5 的 fight/pk 段跨房 tell 开战信号
    foreach ($lane in $ActiveLanes) {
        if (-not $lane.WitnessId) { continue }
        $witLane = @($ActiveLanes | Where-Object { $_.Id -eq $lane.WitnessId }) | Select-Object -First 1
        $witAcct = $null
        if (Test-Path $witLane.VarsFile) {
            $m = Select-String -Path $witLane.VarsFile -Pattern '^acct=(.+)$' |
                Select-Object -Last 1
            if ($m) { $witAcct = $m.Matches[0].Groups[1].Value.Trim() }
        }
        if (-not $witAcct) {
            throw "注册后未取到 L$($witLane.Id) 的 acct（$($witLane.VarsFile)），无法注入 spectator_acct"
        }
        Add-Content -Encoding ASCII $lane.VarsFile "spectator_acct=$witAcct"
    }

    # --- 阶段 2：并行开跑各道 -------------------------------------------------
    Write-Host "-- 阶段 2：并行开跑 $($ActiveLanes.Count) 条道"
    $t0 = Get-Date
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
    $tLaneSec = [int]((Get-Date) - $t0).TotalSeconds

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
    Stop-ServerGraceful -Port $Port

    # 崩溃检测：停服后 diff dump 快照。崩溃发生在关服路径（WM_DESTROY →
    # wait_for_all_worker_threads 期间工作线程仍在跑），剧本结果此时早已判定，
    # 不在这里检查就会「剧本全绿、服务器其实崩了」。
    # 新 dump **原地留在仓库根**，只打印完整路径——挪走（如挪到 %TEMP%）会让人
    # 找不到崩溃证据、误以为崩溃已修好。旧 dump 已在启动前归档到 crash_dumps\。
    $crashDumps = @()
    if ($null -ne $dumpsBefore) {
        $crashDumps = @(Get-ChildItem -Path $RepoRoot -Filter "*.dmp" -File -ErrorAction SilentlyContinue |
                Where-Object { $dumpsBefore -notcontains $_.FullName })
    }
    if ($crashDumps.Count -gt 0) {
        $status = "fail"
        $reason = "服务器崩溃（未处理异常），新增 dump: " +
        (($crashDumps | ForEach-Object { $_.FullName }) -join "、")
        if ($failReason) { $failReason = "$failReason；$reason" } else { $failReason = $reason }
        Write-Warning "服务器崩溃！新 dump 保留在仓库根（未移动）："
        foreach ($d in $crashDumps) {
            Write-Host ("       {0}（{1:N0} 字节，写于 {2}）" -f $d.FullName, $d.Length, $d.LastWriteTime)
        }
        Write-Host "       旧 dump 归档目录：$OldDumpArchive"
    }

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
        $tExportSec = [int]((Get-Date) - $occStart).TotalSeconds
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
            if (Test-CoverageSkippedFile $fn) { continue }
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

# --- 耗时监控：与前次同类通过运行对比、归因、记录（文首第 8 条） ----------------
# 只记录 PASS：失败运行的耗时被中断截断，不可比也不入历史。
$timingLines = @()
$timingReport = $null
if ($status -eq "pass" -and $null -ne $tRegSec -and $null -ne $tLaneSec) {
    $curRecord = [pscustomobject]@{
        time             = Get-Date -Format s
        status           = $status
        workers          = $ActiveLanes.Count
        no_coverage      = [bool]$NoCoverage
        dbg              = [bool]$Dbg
        step_timeout     = $StepTimeout
        build_sec        = $tBuildSec
        server_ready_sec = $tServerReadySec
        reg_sec          = $tRegSec
        lane_sec         = $tLaneSec
        exec_sec         = $tRegSec + $tLaneSec
        export_sec       = $tExportSec
        reg_scenarios    = @(Get-LaneScenarioList "register.sce")
        lanes            = @(foreach ($lane in $ActiveLanes) {
                [pscustomobject]@{
                    id        = [int]$lane.Id
                    desc      = $lane.Desc
                    script    = $lane.Script
                    sec       = if ($lane.Result) { [int]$lane.Result.DurationSec } else { 0 }
                    steps     = [int]$lane.Steps
                    scenarios = @(Get-LaneScenarioList $lane.Script)
                }
            })
    }

    # 同类基线：正序遍历历史（append 时间递增），反复覆盖后留下最近一条同配置 PASS 记录。
    # 必须在 Add-TimingRecord 之前读，否则会拿本次当基线跟自己比。
    $baseline = $null
    foreach ($h in @(Read-TimingHistory)) {
        if ($h.status -ne 'pass') { continue }
        if ([int]$h.workers -ne $curRecord.workers) { continue }
        if ([bool]$h.no_coverage -ne $curRecord.no_coverage) { continue }
        if ([bool]$h.dbg -ne $curRecord.dbg) { continue }
        if ([int]$h.step_timeout -ne $curRecord.step_timeout) { continue }
        $baseline = $h
    }

    $timingReport = Get-TimingReport -Cur $curRecord -Prev $baseline -ThresholdSec $DurationRegressionSec
    $timingLines = @($timingReport.Lines)
    Write-Host ""
    Write-Host "================ 耗时监控 ================"
    foreach ($tl in $timingLines) { Write-Host $tl }
    if ($timingReport.Regression) {
        Write-Host ""
        Write-Host ("!! 总耗时较前次同类运行增长超过阈值 {0}s——归因与拆分探索见上。" -f $DurationRegressionSec)
    }
    Add-TimingRecord -Record $curRecord
}

$lines += "lua-sample E2E：$status"
if ($failReason) { $lines += "reason=$failReason" }
$lines += "workers=$($ActiveLanes.Count)"
foreach ($lane in $ActiveLanes) {
    $lines += ("L{0}={1} name={2} steps={3}{4}" -f $lane.Id, $lane.Status, $lane.Name,
        $lane.Steps, $(if ($lane.Reason) { " reason=$($lane.Reason)" } else { "" }))
}
$lines += "time=$(Get-Date -Format s)"
foreach ($tl in $timingLines) { $lines += $tl }

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
    [void]$listOut.Add("# 由 run-integration-tests.ps1 生成，口径与局限见 docs\design\integration-test.md §5.3")
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

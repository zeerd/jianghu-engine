# reload_testbed.ps1 — 热重载试验台（独立服 6613 + 私有脚本副本 + 一条剧本链）
#
# 职责边界（与剧本的分工）：
#   · 剧本（scenarios\reload\reload_suite.sce）负责「改文件 → 等重载结果
#     （sys 广播）→ 断言新行为」——filecopy/filedelete 写在片段里，因果同处
#     一片段；
#   · 本脚本只做剧本做不到的基础设施与**只有日志能做的断言**：
#       1. 每轮重建私有脚本副本（整树复制 lua-sample + 覆盖 fixture A 变体）
#          与独立库——上一轮的坏文件残留会让本轮开服即报错，必须整树重建；
#       2. 起停独立服（--log-run-id <id>-rl 落同目录另一份日志，主流程的
#          增量扫描与白名单判定按目录快照自动覆盖它）；
#       3. 跑剧本链（单 CLI 进程，单账号巫师自举，无注册）；
#       4. 服务端日志断言：自动重载行确实出现过（机制在跑）、noop 探针从未
#          被误重载、失败尝试确实发生（锚点 ERROR 行在案）；
#       5. 结果 JSON：主流程并入失败判定四通道。
#
# 覆盖率：-NoCoverage 直接拉起；否则经 OCC 起服并导出单路二进制
# coverage_history\e2e_reload.cov，最终由主流程合并通行证并入总报告。
#
# 两种跑法（路径全部从 $PSScriptRoot 推导，均以仓库根为工作目录）：
#   · 由 run-integration-tests.ps1 以 Start-Job -FilePath 调起（标准路径，
#     与各道并行）；
#   · 直接前台跑（调试入口，快速迭代剧本）：
#       powershell -NoProfile -ExecutionPolicy Bypass -File `
#         tests\integration\reload_testbed.ps1 -NoCoverage
#     （需先有 bin\e2e\jianghu.exe 与 bin\cli\jianghu_cli.exe）
param(
    # Start-Job 以位置参数传入：switch 默认不吃位置槽，必须显式声明 Position
    [Parameter(Position = 0)][string]$ServerExe = "",
    [Parameter(Position = 1)][string]$OccExe = "C:\Program Files\OpenCppCoverage\OpenCppCoverage.exe",
    [Parameter(Position = 2)][string]$CliExe = "",
    [Parameter(Position = 3)][switch]$NoCoverage,
    [Parameter(Position = 4)][switch]$Dbg,
    [Parameter(Position = 5)][int]$StepTimeout = 20000,
    # 主流程传入本轮 run id（文件名定位用）；直跑时用固定值便于查日志
    [Parameter(Position = 6)][string]$LogRunId = "reload-dev",
    [Parameter(Position = 7)][string]$ResultFile = "",
    # 由主流程注入：Start-Job 以 ScriptBlock 方式加载本文件（绕开子进程
    # 执行策略），此时 $PSScriptRoot 不可用；直跑时留空从本脚本位置推导
    [Parameter(Position = 8)][string]$IntDir = ""
)

$ErrorActionPreference = "Stop"

# --- 路径（tests\integration\ → 仓库根） ------------------------------------
if (-not $IntDir) { $IntDir = $PSScriptRoot }
if (-not $IntDir) { throw "IntDir 未注入，且无法从 `$PSScriptRoot 推导" }
$RepoRoot     = (Resolve-Path (Join-Path $IntDir "..\..")).Path
$ConfPath     = Join-Path $IntDir "config.reload.ini"
$CliConf      = Join-Path $IntDir "cli.reload.conf"
$ScenarioDir  = Join-Path $IntDir "scenarios"
$FixturesDir  = Join-Path $IntDir "reload_fixtures"
$SampleRoot   = Join-Path $RepoRoot "lua-sample"
$WorkDir      = Join-Path $RepoRoot "test_tmp\e2e_lanes"
$ReloadRoot   = Join-Path $RepoRoot "test_tmp\e2e_reload"
$ScriptsRoot  = Join-Path $ReloadRoot "scripts"
$DbPath       = Join-Path $ReloadRoot "e2e.db"
$LogDir       = Join-Path $IntDir "log"
$CovBin       = Join-Path $RepoRoot "coverage_history\e2e_reload.cov"
if (-not $ServerExe) { $ServerExe = Join-Path $RepoRoot "bin\e2e\jianghu.exe" }
if (-not $CliExe)    { $CliExe    = Join-Path $RepoRoot "bin\cli\jianghu_cli.exe" }
if (-not $ResultFile) {
    $ResultFile = Join-Path $WorkDir "reload_testbed.result.json"
}
$VarsFile = Join-Path $WorkDir "reload.vars"
$ChainReport = Join-Path $WorkDir "reload_suite.report"
$ChainLog    = Join-Path $WorkDir "reload_suite.log"

$result = [pscustomobject]@{
    status     = "fail"
    reason     = ""
    steps      = 0
    chain_sec  = 0
    log_checks = @()
}
$serverHandle = $null
$occHandle    = $null

# 参数拼接统一走引号（仓库路径可能含空格）
function Join-Args([string[]]$a)
{
    return (($a | ForEach-Object {
                if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' }
                else { $_ }
            }) -join ' ')
}

# 无窗口启动子进程并异步排空输出（与主流程 Start-HiddenProcess 同构）
function Start-ProcessHidden
{
    param([string]$Exe, [string]$Arguments, [string]$OutFile, [string]$ErrFile)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $Arguments
    $psi.WorkingDirectory = $RepoRoot
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    [void]$p.Start()
    return [pscustomobject]@{
        Proc    = $p
        StdOut  = $p.StandardOutput.ReadToEndAsync()
        StdErr  = $p.StandardError.ReadToEndAsync()
        OutFile = $OutFile
        ErrFile = $ErrFile
    }
}

function Save-ProcessOutput($h, [int]$TimeoutMs = 20000)
{
    try {
        [void]$h.StdOut.Wait($TimeoutMs)
        [void]$h.StdErr.Wait($TimeoutMs)
        if ($h.StdOut.Result) { Set-Content -LiteralPath $h.OutFile -Value $h.StdOut.Result -Encoding UTF8 }
        if ($h.StdErr.Result) { Set-Content -LiteralPath $h.ErrFile -Value $h.StdErr.Result -Encoding UTF8 }
    }
    catch { }
}

function Wait-TestbedPort([int]$Port, [int]$TimeoutSec)
{
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if ((Test-NetConnection -ComputerName 127.0.0.1 -Port $Port -WarningAction SilentlyContinue).TcpTestSucceeded) { return $true }
        Start-Sleep -Milliseconds 300
    }
    return $false
}

$Port = 6613
try {
    foreach ($line in @(Get-Content -LiteralPath $ConfPath -ErrorAction Stop)) {
        if ($line -match '^\s*port\s*=\s*(\d+)') { $Port = [int]$Matches[1]; break }
    }
}
catch {
    $result.reason = "无法读取配置 $ConfPath : $($_.Exception.Message)"
    throw $result.reason
}

function Start-TestbedServer
{
    if ($NoCoverage) {
        $h = Start-ProcessHidden -Exe $ServerExe `
            -Arguments (Join-Args @("--config", $ConfPath, "--log-run-id", "$LogRunId-rl")) `
            -OutFile (Join-Path $WorkDir "reload_server.stdout.log") `
            -ErrFile (Join-Path $WorkDir "reload_server.stderr.log")
        $script:serverHandle = $h
    }
    else {
        $occLog = Join-Path $WorkDir "reload_occ.log"
        $h = Start-ProcessHidden -Exe $OccExe `
            -Arguments (Join-Args @(
                    "--modules", $ServerExe,
                    "--sources", (Join-Path $RepoRoot "src"),
                    "--optimized_build",
                    "--working_dir", $RepoRoot,
                    "--export_type", "binary:$CovBin",
                    "--",
                    $ServerExe, "--config", $ConfPath, "--log-run-id", "$LogRunId-rl"
                )) `
            -OutFile $occLog -ErrFile "$occLog.err"
        $script:occHandle = $h
    }
    if (-not (Wait-TestbedPort $Port 180)) {
        throw "reload 试验台服务器未在端口 $Port 就绪"
    }
    Write-Output "reload 试验台服务器就绪（端口 $Port）"
}

# 优雅停服（认端口不认进程名，按 PID 关窗）→ 覆盖率模式等 OCC 导出
function Stop-TestbedServer
{
    $conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $conn) {
        $p = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
        if ($null -ne $p -and $p.ProcessName -eq 'jianghu') {
            $serverPid = $p.Id
            try { [void]$p.CloseMainWindow() } catch { }
            $deadline = (Get-Date).AddSeconds(30)
            while ((Get-Date) -lt $deadline -and (Get-Process -Id $serverPid -ErrorAction SilentlyContinue)) {
                Start-Sleep -Milliseconds 500
            }
            Get-Process -Id $serverPid -ErrorAction SilentlyContinue | Stop-Process -Force
        }
    }
    if ($script:serverHandle) {
        Save-ProcessOutput $script:serverHandle
        $script:serverHandle = $null
    }
    if ($script:occHandle) {
        $occ = $script:occHandle.Proc
        $deadline = (Get-Date).AddSeconds(180)
        while ((Get-Date) -lt $deadline -and -not $occ.HasExited) { Start-Sleep -Milliseconds 500 }
        if (-not $occ.HasExited) { try { $occ | Stop-Process -Force } catch { } }
        Save-ProcessOutput $script:occHandle
        $script:occHandle = $null
        if (-not $NoCoverage -and -not (Test-Path $CovBin)) {
            throw "reload 试验台覆盖率未导出（$CovBin）"
        }
    }
}

# 跑剧本链：CLI 输出落盘（UTF-8），按剧本报告判定
function Invoke-Chain
{
    $cliArgs = @("--config", $CliConf,
        "--script", (Join-Path $ScenarioDir "reload\reload_suite.sce"),
        "--vars", $VarsFile,
        "--step-timeout", "$StepTimeout",
        "--report", $ChainReport)
    if ($Dbg) { $cliArgs += "--dbg" }
    $h = Start-ProcessHidden -Exe $CliExe -Arguments (Join-Args $cliArgs) `
        -OutFile $ChainLog -ErrFile "$ChainLog.err"
    $h.Proc.WaitForExit()
    Save-ProcessOutput $h
    $exit = $h.Proc.ExitCode

    $rep = @{}
    if (Test-Path $ChainReport) {
        foreach ($l in Get-Content -LiteralPath $ChainReport -Encoding UTF8) {
            if ($l -match '^([^=]+)=(.*)$') { $rep[$Matches[1]] = $Matches[2] }
        }
    }
    $steps = 0
    if (Test-Path $ChainLog) {
        $raw = Get-Content -LiteralPath $ChainLog -Raw -Encoding UTF8
        $m = [regex]::Match($raw, '\[scenario\] PASS:\s*(\d+)\s*steps')
        if ($m.Success) { $steps = [int]$m.Groups[1].Value }
    }
    $ok = ($exit -eq 0 -and $rep['status'] -eq 'pass')
    $reason = $rep['reason']
    if (-not $ok) {
        if (-not $reason) { $reason = "CLI 退出码 $exit" }
        if ($rep['line']) { $reason = "$reason（剧本行 $($rep['line'])）" }
    }
    return [pscustomobject]@{ Ok = $ok; Steps = $steps; Reason = $reason }
}

# 服务端日志断言：剧本只断言客户端可见行为，「重载机制真的在跑 / 探针没被
# 误重载 / 失败尝试真的发生」只有日志能回答。needle 全为 ASCII，日志编码
# 无论 GBK/UTF-8 都不受影响。
function Assert-ServerLog
{
    $checks = @()
    $logFiles = @(Get-ChildItem -LiteralPath $LogDir `
            -Filter "*$LogRunId-rl*.txt" -File -ErrorAction SilentlyContinue)
    if ($logFiles.Count -eq 0) {
        return @([pscustomobject]@{ needle = "<log file>"; want = "present"; ok = $false })
    }
    $raw = ""
    foreach ($f in $logFiles) {
        $raw += [System.IO.File]::ReadAllText($f.FullName)
    }

    $must = @(
        "Lua auto-reload: e2e_reload_npc",
        "Lua auto-reload: e2e_reload_item",
        "Lua auto-reload: sample_mountain_foot",
        "Lua auto-reload: e2e_reload_common",
        "Lua reload loadfile failed [e2e_reload_npc]",
        "Lua reload pcall failed [e2e_reload_npc]",
        "[Lua] e2e_reload_npc: cannot read file"
    )
    foreach ($n in $must) {
        $hit = $raw.Contains($n)
        $checks += [pscustomobject]@{ needle = $n; want = "present"; ok = $hit }
    }
    # noop 探针：原样重写绝不能产生重载行——产生了就是「mtime 变、内容不变」
    # 被误判成变更（djb2 哈希确认失效）
    $probe = "Lua auto-reload: e2e_reload_noop_probe"
    $checks += [pscustomobject]@{ needle = $probe; want = "absent"; ok = (-not $raw.Contains($probe)) }
    # 注意：此处不能用 `return ,$checks`——一元逗号会把数组包成单元素嵌套，
    # Count/JSON 全变 1；函数输出经管道天然展开，调用方 @() 重组即可
    return $checks
}

try {
    # --- 1. 每轮重建私有脚本副本与独立库 ------------------------------------
    # 整树重建而非增量覆盖：上一轮残留的坏文件（语法错/被删）会污染本轮开服。
    if (Test-Path $ScriptsRoot) { Remove-Item -LiteralPath $ScriptsRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $ScriptsRoot | Out-Null
    foreach ($item in @(Get-ChildItem -LiteralPath $SampleRoot)) {
        Copy-Item -LiteralPath $item.FullName -Destination $ScriptsRoot -Recurse -Force
    }
    # fixture A 变体覆盖到副本（起始房间顶替、NPC/物品/共享模块/探针就位）
    foreach ($item in @(Get-ChildItem -LiteralPath (Join-Path $FixturesDir "a"))) {
        Copy-Item -LiteralPath $item.FullName -Destination $ScriptsRoot -Recurse -Force
    }
    foreach ($f in @($DbPath, "$DbPath-wal", "$DbPath-shm")) {
        if (Test-Path $f) { Remove-Item -LiteralPath $f -Force }
    }
    if (Test-Path $CovBin) { Remove-Item -LiteralPath $CovBin -Force }
    Write-Output "私有脚本副本与独立库已重建"

    # --- 2. 剧本链的输入变量（巫师自举，免注册） ----------------------------
    # 巫师密码取 config [wizard] password（缺省 12345678，与服务端
    # ensure_wizard_accounts 同缺省）。路径用仓库根相对路径：CLI 以仓库根为
    # 工作目录，filecopy 的落点校验（test_tmp/ 白名单）按相对路径判定。
    $wizardPass = "12345678"
    $inWizard = $false
    foreach ($cfgLine in @(Get-Content -LiteralPath $ConfPath)) {
        if ($cfgLine -match '^\s*\[([^\]]+)\]') { $inWizard = ($Matches[1] -eq 'wizard') }
        elseif ($inWizard -and $cfgLine -match '^\s*password\s*=\s*(.+?)\s*$') { $wizardPass = $Matches[1] }
    }
    $varsText = "acct=1`npass=$wizardPass`n" +
    "fixtures=tests/integration/reload_fixtures`n" +
    "scripts=test_tmp/e2e_reload/scripts`n"
    [System.IO.File]::WriteAllText($VarsFile, $varsText,
        (New-Object System.Text.UTF8Encoding($false)))

    # --- 3. 起服 → 跑链 → 停服 ----------------------------------------------
    $t0 = Get-Date
    Start-TestbedServer

    Write-Output "跑热重载剧本链（reload\reload_suite.sce）..."
    $chain = Invoke-Chain
    $result.steps = $chain.Steps
    $result.chain_sec = [int]((Get-Date) - $t0).TotalSeconds

    Stop-TestbedServer

    if (-not $chain.Ok) { throw "剧本链失败：$($chain.Reason)" }
    Write-Output "剧本链 PASS（$($chain.Steps) steps，$($result.chain_sec)s）"

    # --- 4. 服务端日志断言 --------------------------------------------------
    $result.log_checks = @(Assert-ServerLog)
    $failedChecks = @($result.log_checks | Where-Object { -not $_.ok })
    if ($failedChecks.Count -gt 0) {
        $detail = @($failedChecks | ForEach-Object {
                "$($_.needle)（期望 $($_.want)）"
            }) -join "；"
        throw "服务端日志断言失败：$detail"
    }
    Write-Output "服务端日志断言通过（$($result.log_checks.Count) 项）"

    $result.status = "pass"
}
catch {
    $result.reason = $_.Exception.Message
    # 直跑调试时这是唯一的失败出口：必须吵，别让人对着空白控制台猜
    Write-Output "reload 试验台失败：$($result.reason)"
    try { Stop-TestbedServer } catch {
        if (-not $result.reason) { $result.reason = $_.Exception.Message }
    }
}
finally {
    # 兜底：任何路径都回收端口上的试验台实例（认端口不认进程名）
    try {
        $conn = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $conn) {
            $p = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
            if ($null -ne $p -and $p.ProcessName -eq 'jianghu') {
                try { [void]$p.CloseMainWindow() } catch { }
                Start-Sleep -Milliseconds 1500
                Get-Process -Id $p.Id -ErrorAction SilentlyContinue | Stop-Process -Force
            }
        }
    }
    catch { }
    try {
        $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $ResultFile -Encoding UTF8
    }
    catch { }
}

# start-stress.ps1 — 读账号账本前 N 行，拉起 N 个 bot 进程随机行动。
#
# 用法（仓库根）：
#   .\stress\start-stress.ps1 [-Count 100] [-DurationSec 0]
#     -Count       账号数（取账本前 N 行），卡了就减小重跑
#     -DurationSec 跑够秒数自动收尾；0 = 一直跑，Ctrl+C 收尾
#
# 每账号一个 jianghu_cli --bot 进程（一条 TCP 连接），错峰登录；
# 输出各自重定向到 stress\logs\bot_<acct>.log。收尾只按本脚本持有的
# PID 停（进程安全铁律：认句柄不认进程名）。

[CmdletBinding()]
param(
    [int]$Count = 100,
    [int]$DurationSec = 0,
    [int]$StaggerMs = 300,
    [string]$CliExe = (Join-Path $PSScriptRoot "..\bin\cli\x64\jianghu_cli.exe"),
    [string]$CliConf = (Join-Path $PSScriptRoot "cli.conf"),
    [string]$AccountsFile = (Join-Path $PSScriptRoot "accounts.tsv"),
    [string]$LogDir = (Join-Path $PSScriptRoot "logs")
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path -LiteralPath $AccountsFile)) {
    throw "账号账本不存在：$AccountsFile（先跑 register-stress.ps1）"
}
if (-not (Test-Path -LiteralPath $CliExe)) { throw "CLI 不存在：$CliExe（先构建 jianghu_cli）" }
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

# 读账本前 N 行：acct<TAB>pass<TAB>name
$accounts = @()
foreach ($line in [IO.File]::ReadAllLines($AccountsFile, [Text.Encoding]::UTF8)) {
    if ($line -match "^(\d+)\t([^\t]+)\t(.+)$") {
        $accounts += [pscustomobject]@{ Acct = $Matches[1]; Pass = $Matches[2]; Name = $Matches[3] }
    }
    if ($accounts.Count -ge $Count) { break }
}
if ($accounts.Count -eq 0) { throw "账本里没有可用账号行" }
if ($accounts.Count -lt $Count) {
    Write-Warning "账本只有 $($accounts.Count) 个账号，少于请求的 $Count，按已有数量拉起"
}

$procs = @()
try {
    # 错峰登录：瞬时 100 连接既撞每 IP 上限的判定窗口，也淹没服务端；
    # 每 300ms 起一个，100 个约 30 秒全部上线。
    foreach ($a in $accounts) {
        $log = Join-Path $LogDir "bot_$($a.Acct).log"
        $p = Start-Process -FilePath $CliExe -PassThru -WindowStyle Hidden `
            -ArgumentList @("--config", $CliConf, "--bot",
                "--account", $a.Acct, "--password", $a.Pass) `
            -RedirectStandardOutput $log -RedirectStandardError "$log.err" `
            -WorkingDirectory (Split-Path $CliExe -Parent)
        $procs += $p
        Start-Sleep -Milliseconds $StaggerMs
    }
    Write-Output "已拉起 $($procs.Count) 个 bot（日志 $LogDir）"
    Write-Output "观察卡顿：人工看游戏内表现，或本机 perf/资源监视器盯 jianghu.exe"

    $deadline = if ($DurationSec -gt 0) { (Get-Date).AddSeconds($DurationSec) } else { $null }
    while ($true) {
        Start-Sleep -Seconds 5
        $alive = @($procs | Where-Object { -not $_.HasExited })
        if ($null -ne $deadline -and (Get-Date) -ge $deadline) {
            Write-Output "到时（$DurationSec s），收尾"
            break
        }
        if ($alive.Count -ne $procs.Count) {
            Write-Warning "有 bot 退出：存活 $($alive.Count)/$($procs.Count)，详见 $LogDir"
        }
        if ($alive.Count -eq 0) { Write-Warning "全部 bot 已退出"; break }
    }
}
finally {
    # 只停本脚本启动且仍存活的 PID（自有子进程，按句柄回收）
    foreach ($p in $procs) {
        if (-not $p.HasExited) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch { }
        }
    }
    Write-Output "收尾完成：$($procs.Count) 个 bot 进程已回收"
}

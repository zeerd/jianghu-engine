# register-stress.ps1 — 批量注册压测账号，落 accounts.tsv。
#
# 用法（仓库根）：
#   .\stress\register-stress.ps1 [-Count 100] [-Parallel 8]
#
# 去重账本是 accounts.tsv（acct<TAB>pass<TAB>name）：已登记的名字直接跳过，
# 可反复补跑。名字 = 中文数字 + 号（一号…九百九十九号）——服务端名字校验
# 只认中文/英文字母（dataset data/login.lua check_name，数字被拒）且字节
# 上限 12，中文数字方案在 1..999 恒合法、恒唯一。
#
# 注册走现成 register.sce（游客登录 → 注册专员分步菜单 → capture acct），
# 每账号一份 vars 注入 name/pass/email/gender/attrs。

[CmdletBinding()]
param(
    [int]$Count = 100,
    [int]$Parallel = 8,
    [string]$CliExe = (Join-Path $PSScriptRoot "..\bin\cli\x64\jianghu_cli.exe"),
    [string]$CliConf = (Join-Path $PSScriptRoot "cli.conf"),
    [string]$AccountsFile = (Join-Path $PSScriptRoot "accounts.tsv"),
    [string]$RegisterSce = (Join-Path $PSScriptRoot "..\tests\integration\scenarios\register.sce"),
    [string]$WorkDir = (Join-Path $PSScriptRoot "tmp"),
    [string]$Password = "stress1234"
)

$ErrorActionPreference = "Stop"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

# 中文数字 1..999：一号、二十一号、一百零一号、九百九十九号
function ConvertTo-ChineseNumber([int]$n) {
    $digits = "零一二三四五六七八九"
    if ($n -lt 10) { return $digits[$n].ToString() }
    if ($n -ge 100) {
        $s = $digits[[int][math]::Floor($n / 100)].ToString() + "百"
        $rem = $n % 100
        if ($rem -eq 0) { return $s }
        if ($rem -lt 10) { return $s + "零" + $digits[$rem].ToString() }
        # 10..19 段接在百位后读「一十五」（一百一十五），20+ 段直接接
        if ($rem -lt 20) { return $s + "一" + (ConvertTo-ChineseNumber $rem) }
        return $s + (ConvertTo-ChineseNumber $rem)
    }
    if ($n -lt 20) {
        $tail = if ($n % 10 -ne 0) { $digits[$n % 10].ToString() } else { "" }
        return "十$tail"
    }
    $s = $digits[[int][math]::Floor($n / 10)].ToString() + "十"
    $rest = $n % 10
    if ($rest -ne 0) { $s += $digits[$rest].ToString() }
    return $s
}

if ($Count -lt 1 -or $Count -gt 999) {
    throw "Count 须在 1..999：名字 = 中文数字 + 号，超过 999 会突破 12 字节名字上限"
}
foreach ($p in @($CliExe, $CliConf, $RegisterSce)) {
    if (-not (Test-Path -LiteralPath $p)) { throw "找不到文件：$p" }
}
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

# 读账本：已有名字集合 + 已登记行数
$registered = @{}
if (Test-Path -LiteralPath $AccountsFile) {
    foreach ($line in [IO.File]::ReadAllLines($AccountsFile, [Text.Encoding]::UTF8)) {
        if ($line -match "^(\d+)\t([^\t]+)\t(.+)$") {
            $registered[$Matches[3]] = $Matches[1]
        }
    }
}
Write-Output ("账本已有 {0} 个账号（{1}）" -f $registered.Count, $AccountsFile)

# 生成缺口名单
$pending = @()
for ($i = 1; $i -le $Count; $i++) {
    $name = (ConvertTo-ChineseNumber $i) + "号"
    if (-not $registered.ContainsKey($name)) { $pending += $name }
}
if ($pending.Count -eq 0) {
    Write-Output "全部 $Count 个账号已注册，无需补跑。"
    return
}
Write-Output "待注册 $($pending.Count) 个（并行度 $Parallel）"

# 分批注册：每批并行起 N 个 CLI 进程跑 register.sce，等批完成收 acct 追加账本
$failures = @()
for ($begin = 0; $begin -lt $pending.Count; $begin += $Parallel) {
    $batch = @($pending[$begin..([math]::Min($begin + $Parallel - 1, $pending.Count - 1))])
    $jobs = @()
    foreach ($name in $batch) {
        $tag = [IO.Path]::GetFileNameWithoutExtension([IO.Path]::GetRandomFileName())
        $vars = Join-Path $WorkDir "reg_$tag.vars"
        $report = Join-Path $WorkDir "reg_$tag.report"
        $log = Join-Path $WorkDir "reg_$tag.log"
        # 变量含中文角色名：UTF-8 无 BOM（CLI load_vars 按原始字节切 key）
        $varsText = "name=$name`npass=$Password`nemail=stress$([array]::IndexOf($pending, $name))@example.com`ngender=男性`nattrs=21 21 21 21`n"
        [IO.File]::WriteAllText($vars, $varsText, $utf8NoBom)
        $proc = Start-Process -FilePath $CliExe -PassThru -WindowStyle Hidden `
            -ArgumentList @("--config", $CliConf, "--script", $RegisterSce,
                "--vars", $vars, "--report", $report) `
            -RedirectStandardOutput $log -RedirectStandardError "$log.err" `
            -WorkingDirectory (Split-Path $CliExe -Parent)
        $jobs += [pscustomobject]@{
            Name = $name; Proc = $proc; Vars = $vars; Report = $report; Log = $log
        }
    }
    foreach ($j in $jobs) { $j.Proc.WaitForExit() }

    # 逐个收账：report=pass + vars 有 acct 才算成功。
    # 不看 Process.ExitCode：Start-Process -PassThru 的对象在本机
    # WaitForExit 后仍取不到 ExitCode（返回空），report 是 CLI 自己写的，
    # 才是权威判据。
    $appends = @()
    foreach ($j in $jobs) {
        $acct = $null
        $pass = $false
        if (Test-Path -LiteralPath $j.Report) {
            $rep = Get-Content -LiteralPath $j.Report -Encoding UTF8 |
                Where-Object { $_ -match '^(status|reason)=' }
            if ($rep -match 'status=pass') { $pass = $true }
        }
        if ($pass -and (Test-Path -LiteralPath $j.Vars)) {
            $m = Select-String -LiteralPath $j.Vars -Pattern '^acct=(\d+)' | Select-Object -Last 1
            if ($m) { $acct = $m.Matches[0].Groups[1].Value } else { $pass = $false }
        }
        if ($pass) {
            $appends += "$acct`t$Password`t$($j.Name)"
        }
        else {
            $reason = "report 缺失或非 pass"
            if (Test-Path -LiteralPath $j.Report) {
                $rep = Get-Content -LiteralPath $j.Report -Encoding UTF8 |
                    Where-Object { $_ -match '^(status|reason)=' }
                $reason = ($rep -join "; ")
            }
            $failures += "$($j.Name)：$reason（日志 $($j.Log)）"
        }
        # 中间文件收进 tmp 归档，不留在账本旁
        foreach ($f in @($j.Vars, $j.Report)) {
            if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue }
        }
    }
    if ($appends.Count -gt 0) {
        [IO.File]::AppendAllText($AccountsFile, (($appends -join "`r`n") + "`r`n"), $utf8NoBom)
    }
    $done = [math]::Min($begin + $batch.Count, $pending.Count)
    Write-Output "进度 $done/$($pending.Count)（本批成功 $($appends.Count)/$($batch.Count)）"
}

if ($failures.Count -gt 0) {
    Write-Output "失败 $($failures.Count) 个："
    $failures | ForEach-Object { Write-Output "  $_" }
    exit 1
}
Write-Output "注册完成，账本：$AccountsFile"
exit 0

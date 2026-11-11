<#
.SYNOPSIS
    统计 LuaBind.cpp 绑定函数在各 Lua 脚本根中的调用次数，列出低使用量绑定。

.DESCRIPTION
    解析 src/lua/LuaBind.cpp 的 object_methods[] 与 global_funcs[]，在脚本根中统计每个
    绑定名的调用点：剥离注释与字符串字面量后，对象方法按 x:foo( / x.foo( 计，全局按独立
    foo( 计；排除 function foo( 定义与 table.remove / math.random 等标准库成员调用。
    tests/ 集测剧本单列，不计入门槛。

.PARAMETER DatasetRoot
    数据集脚本根，默认 C:\jianghu-dataset；不存在时跳过该根并告警。

.PARAMETER Threshold
    低使用量门槛（内容层调用次数 < 阈值），默认 10。

.PARAMETER OutFile
    明细输出路径，默认 <repo>/test_tmp/bind_usage_detail.txt（test_tmp 已 git 忽略；
    绑定名清单由 check-binds.ps1 自带的 usage 扫描写到 test_tmp/bind_usage.txt）。

.EXAMPLE
    .\tests\count_bind_usage.ps1
    .\tests\count_bind_usage.ps1 -Threshold 20
#>
[CmdletBinding()]
param(
    [string]$DatasetRoot = 'C:\jianghu-dataset',
    [int]$Threshold = 10,
    [string]$OutFile
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutFile) { $OutFile = Join-Path $repoRoot 'test_tmp\bind_usage_detail.txt' }

$roots = [ordered]@{
    sample  = Join-Path $repoRoot 'lua-sample'
    default = Join-Path $repoRoot 'lua-default'
    tests   = Join-Path $repoRoot 'tests'
}
if (Test-Path $DatasetRoot) {
    $roots['dataset'] = $DatasetRoot
} else {
    Write-Warning "dataset root not found, skipped: $DatasetRoot"
}

# 1) 解析绑定表
$bindFile = Join-Path $repoRoot 'src\lua\LuaBind.cpp'
$src      = [IO.File]::ReadAllText($bindFile)
$omBlock  = [regex]::Match($src, 'object_methods\[\]\s*=\s*\{(.*?)\};', 'Singleline').Groups[1].Value
$gfBlock  = [regex]::Match($src, 'global_funcs\[\]\s*=\s*\{(.*?)\};', 'Singleline').Groups[1].Value
$objectMethods = @([regex]::Matches($omBlock, '\{"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
$globalFuncs   = @([regex]::Matches($gfBlock, '\{"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })

$names = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($n in $objectMethods) { [void]$names.Add($n) }
foreach ($n in $globalFuncs)   { [void]$names.Add($n) }

# 2) 统计
$strip = [regex]'(--\[\[[\s\S]*?\]\]|--[^\n]*|"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*'')'
$call  = [regex]'\b([A-Za-z_]\w*)\s*\('
$globalDef = [regex]'(?i)(local\s+)?function\s+$'
$methodDef = [regex]'(?i)function\s+\S+\s*$'

$stats = @{}
foreach ($n in $names) { $stats[$n] = @{}; foreach ($k in $roots.Keys) { $stats[$n][$k] = 0 } }

foreach ($kv in $roots.GetEnumerator()) {
    $bucket = $kv.Key
    $files = Get-ChildItem -Path $kv.Value -Recurse -Include *.lua -File -ErrorAction SilentlyContinue
    foreach ($f in $files) {
        $text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($f.FullName))
        $text = $strip.Replace($text, ' ')
        foreach ($m in $call.Matches($text)) {
            $id = $m.Groups[1].Value
            if (-not $names.Contains($id)) { continue }
            $idx = $m.Index
            $pre = if ($idx -gt 0) { $text[$idx - 1] } else { ' ' }
            if ($pre -eq '.' -or $pre -eq ':') {
                if ($id -notin $objectMethods) { continue }
                # 排除 Lua 标准库成员调用（table.remove 等）
                $ctxStart = [Math]::Max(0, $idx - 30)
                $preCtx = $text.Substring($ctxStart, $idx - $ctxStart)
                if (($preCtx -replace '[.:]\s*$', '') -match '(?i)\b(table|string|math|os|io|coroutine|debug)$') { continue }
                # 排除方法定义 function obj:name(
                if ($pre -eq ':') {
                    $before = $text.Substring(0, $idx - 1)
                    if ($before.Length -gt 60) { $before = $before.Substring($before.Length - 60) }
                    if ($methodDef.IsMatch($before)) { continue }
                }
            } elseif ([int]$pre -le 32 -or $pre -in @('(', '=', ',', ';', '{')) {
                if ($id -notin $globalFuncs) { continue }
                $before = $text.Substring(0, $idx)
                if ($before.Length -gt 60) { $before = $before.Substring($before.Length - 60) }
                if ($globalDef.IsMatch($before)) { continue }
            } else { continue }
            $stats[$id][$bucket]++
        }
    }
}

# 3) 输出：内容层（sample/default/dataset）调用次数升序，门槛 < Threshold
$contentRoots = @($roots.Keys | Where-Object { $_ -ne 'tests' })
$rows = foreach ($n in $names) {
    $s = $stats[$n]
    $content = ($contentRoots | ForEach-Object { $s[$_] } | Measure-Object -Sum).Sum
    $scope = @()
    if ($n -in $objectMethods) { $scope += 'obj' }
    if ($n -in $globalFuncs)   { $scope += 'global' }
    $row = [ordered]@{ name = $n; scope = $scope -join '+' }
    foreach ($k in $roots.Keys) { $row[$k] = $s[$k] }
    $row['content'] = $content
    [pscustomobject]$row
}

$low = @($rows | Where-Object { $_.content -lt $Threshold })
$summary = "=== 绑定总数: $($names.Count) ; 内容层调用 < $Threshold 的: $($low.Count) ==="
$summary
$low | Sort-Object content, name | Format-Table -AutoSize | Out-String -Width 200

if (Test-Path (Split-Path -Parent $OutFile) -PathType Container) {
    ($summary + "`r`n`r`n" + (($rows | Sort-Object content, name | Format-Table -AutoSize | Out-String -Width 200))) |
        Out-File -FilePath $OutFile -Encoding utf8
    "明细已写入: $OutFile"
}

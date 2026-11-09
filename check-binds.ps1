# check-binds.ps1 -- Lua binding reconciliation + script-side usage scan.
#
# Parses src/lua/LuaBind.cpp registration tables and cross-checks four sides:
#   table entries <-> LuaBindDecls.h <-> `int l_*(lua_State*` impls <-> tests.
# Then scans the three call-site repos (dataset / lua-sample / lua-default) for
# usage of every registered Lua name and writes a report plus baseline CSVs.
#
# Usage:
#   .\check-binds.ps1                 # full run (reconcile + scan)
#   .\check-binds.ps1 -SkipScan       # reconciliation only (fast)
#   .\check-binds.ps1 -DatasetRoot D:\other-dataset
#
# Outputs (test_tmp/):
#   bind_usage.txt   strict-usage report, sorted by strict call count
#   bind_full.csv    name,tables,func,impl,full_hits,strict_hits
#   bind_strict.csv  one row per strict call site: name,file,line
#   bind_usage2.csv  name,dataset,sample,default,total (strict counts/repo)
#
# Strict call-site rule: comment-only lines dropped, `function <path>name(`
# definition tokens dropped, and a name preceded by `.` (field access on a
# plain Lua table, e.g. `zhangmen.prevent_learn(`) is not a call into the
# binding table. Bare `name(` and colon method calls count. Counts are per
# line: a line where the same name appears twice is one call site.
param(
    [string]$DatasetRoot = 'c:\jianghu-dataset',
    [switch]$SkipScan
)

$ErrorActionPreference = 'Stop'
$repo = $PSScriptRoot
$bindSrc = Join-Path $repo 'src\lua\LuaBind.cpp'
$declSrc = Join-Path $repo 'src\lua\LuaBindDecls.h'
$outDir = Join-Path $repo 'test_tmp'
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

# ---------------------------------------------------------------- tables ----
$text = [IO.File]::ReadAllText($bindSrc)
$globalAt = $text.IndexOf('static const luaL_Reg global_funcs')
if ($globalAt -lt 0) { throw 'global_funcs[] not found in LuaBind.cpp' }

$entryRx = [regex]'\{\s*"([^"]+)"\s*,\s*(l_[A-Za-z0-9_]+)\s*\}'
$entries = @()   # @{table;name;func;line}
foreach ($m in $entryRx.Matches($text)) {
    $table = if ($m.Index -lt $globalAt) { 'object' } else { 'global' }
    $line = ($text.Substring(0, $m.Index) -split "`n").Count
    $entries += [pscustomobject]@{
        table = $table; name = $m.Groups[1].Value
        func = $m.Groups[2].Value; line = $line
    }
}

$methodCount = @($entries | Where-Object table -eq 'object').Count
$globalCount = @($entries | Where-Object table -eq 'global').Count
# names/funcs are case-sensitive Lua identifiers (Query ~= query)
$seenNames = New-Object 'System.Collections.Generic.HashSet[string]' (
    [StringComparer]::Ordinal)
$names = @($entries | ForEach-Object {
        if ($seenNames.Add($_.name)) { $_.name }
    })
$seenFuncs = New-Object 'System.Collections.Generic.HashSet[string]' (
    [StringComparer]::Ordinal)
$funcs = @($entries | ForEach-Object {
        if ($seenFuncs.Add($_.func)) { $_.func }
    })

Write-Host '=== Registration tables (src/lua/LuaBind.cpp) ==='
Write-Host ("entries : {0}  (object_methods {1} + global_funcs {2})" -f
    $entries.Count, $methodCount, $globalCount)
Write-Host ("names   : {0}" -f $names.Count)
Write-Host ("funcs   : {0}" -f $funcs.Count)

# ------------------------------------------------------------- warnings ----
$dupes = @()
foreach ($g in ($entries | Group-Object table, name -CaseSensitive)) {
    if ($g.Count -gt 1) {
        $dupes += ("{0}.{1} x{2} at lines {3}" -f
            $g.Group[0].table, $g.Group[0].name, $g.Count,
            (($g.Group | ForEach-Object line) -join ','))
    }
}
$multiName = @()
foreach ($g in ($entries | Group-Object func)) {
    $ns = @($g.Group | Sort-Object name -CaseSensitive -Unique |
        ForEach-Object name)
    if ($ns.Count -gt 1) {
        $multiName += ("{0} <- {1}" -f $g.Name, ($ns -join ' | '))
    }
}
$PascalRx = [regex]'[A-Z]'
$pascalNames = @($names | Where-Object { $PascalRx.IsMatch($_) } | Sort-Object)

Write-Host ''
Write-Host '=== Warnings ==='
Write-Host ("duplicate registrations : {0}" -f $dupes.Count)
$dupes | ForEach-Object { Write-Host ('  DUP  ' + $_) }
Write-Host ("functions with >1 name  : {0}" -f $multiName.Count)
$multiName | ForEach-Object { Write-Host ('  ALIAS ' + $_) }
Write-Host ("PascalCase names        : {0}" -f $pascalNames.Count)
Write-Host ('  ' + ($pascalNames -join ', '))

# ------------------------------------------------------ four-way check -----
$declText = [IO.File]::ReadAllText($declSrc)
$declRx = [regex]'(?m)^int\s+(l_[A-Za-z0-9_]+)\s*\(\s*lua_State'
$decls = @{}    # func -> 'src\...h:line'
foreach ($m in $declRx.Matches($declText)) {
    $f = $m.Groups[1].Value
    if (-not $decls.ContainsKey($f)) {
        $ln = ($declText.Substring(0, $m.Index) -split "`n").Count
        $decls[$f] = ('src\lua\LuaBindDecls.h:' + $ln)
    }
}

$implRx = [regex]'(?m)^int\s+(l_[A-Za-z0-9_]+)\s*\(\s*lua_State'
$impls = @{}    # func -> 'src\xxx.cpp:line'
Get-ChildItem (Join-Path $repo 'src') -Recurse -Include *.cpp, *.h |
    ForEach-Object {
        $f = $_.FullName
        $src = [IO.File]::ReadAllText($f)
        foreach ($m in $implRx.Matches($src)) {
            $fn = $m.Groups[1].Value
            if (-not $impls.ContainsKey($fn)) {
                $ln = ($src.Substring(0, $m.Index) -split "`n").Count
                $rel = $f.Substring($repo.Length + 1)
                $impls[$fn] = ('{0}:{1}' -f $rel, $ln)
            }
        }
    }

$testRefs = @{}    # func -> [list of test files]
Get-ChildItem (Join-Path $repo 'tests\unit') -Filter *.cpp | ForEach-Object {
    $rel = $_.Name
    $src = [IO.File]::ReadAllText($_.FullName)
    $src = (($src -split "`r?`n") | ForEach-Object {
            $i = $_.IndexOf('//')
            if ($i -ge 0) { $_.Substring(0, $i) } else { $_ }
        }) -join "`n"
    foreach ($m in ([regex]'\bl_[A-Za-z0-9_]+').Matches($src)) {
        $fn = $m.Value
        if (-not $testRefs.ContainsKey($fn)) { $testRefs[$fn] = @() }
        if ($testRefs[$fn] -notcontains $rel) { $testRefs[$fn] += $rel }
    }
}

$problems = @()
foreach ($e in $entries) {
    if (-not $decls.ContainsKey($e.func)) {
        $problems += ('table entry {0} ({1}) has no decl in LuaBindDecls.h' -f
            $e.name, $e.func)
    }
    if (-not $impls.ContainsKey($e.func)) {
        $problems += ('table entry {0} ({1}) has no int l_*(lua_State* impl' -f
            $e.name, $e.func)
    }
}
foreach ($f in ($decls.Keys | Sort-Object)) {
    if (-not $impls.ContainsKey($f)) {
        $problems += ('decl {0} ({1}) has no impl' -f $f, $decls[$f])
    }
    if ($funcs -notcontains $f) {
        $problems += ('decl {0} ({1}) is not registered in either table' -f
            $f, $decls[$f])
    }
}
foreach ($f in ($testRefs.Keys | Sort-Object)) {
    if (-not $impls.ContainsKey($f)) {
        $problems += ('test reference {0} ({1}) has no impl' -f
            $f, (($testRefs[$f]) -join ','))
    }
}
# impls outside both tables are internal helpers (e.g. lua_pushcclosure
# targets); listed for review, mismatches only if they are bound
$unbound = @($impls.Keys | Where-Object { $funcs -cnotcontains $_ } |
    Sort-Object)
$untested = @($impls.Keys | Where-Object { -not $testRefs.ContainsKey($_) } |
    Sort-Object)

Write-Host ''
Write-Host '=== Four-way reconciliation ==='
if ($problems.Count -eq 0) {
    Write-Host 'OK: table <-> decl <-> impl <-> tests are consistent'
} else {
    $problems | ForEach-Object { Write-Host ('  MISMATCH ' + $_) }
}
Write-Host ('impls never referenced by tests/unit/*.cpp : {0}' -f
    $untested.Count)
if ($untested.Count -gt 0) {
    Write-Host ('  ' + ($untested -join ', '))
}
Write-Host ('impls outside both tables (internal helpers) : {0}' -f
    $unbound.Count)
if ($unbound.Count -gt 0) {
    Write-Host ('  ' + ($unbound -join ', '))
}

if ($SkipScan) {
    exit $(if ($problems.Count -gt 0) { 1 } else { 0 })
}

# ----------------------------------------------------------- usage scan ----
$roots = @(
    @{ key = 'dataset'; path = $DatasetRoot },
    @{ key = 'sample'; path = (Join-Path $repo 'lua-sample') },
    @{ key = 'default'; path = (Join-Path $repo 'lua-default') }
)

$nameSet = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($n in $names) { [void]$nameSet.Add($n) }

# identifier immediately followed by '(' ; lookbehind keeps the match from
# starting inside a longer identifier.
$callRx = [regex]'(?<![A-Za-z0-9_])(?<id>[A-Za-z_][A-Za-z0-9_]*)\s*\('
$defRx = [regex]'function\s+[A-Za-z0-9_.:]*\s*\('
$commentRx = [regex]'^\s*--'

$fullHits = New-Object 'System.Collections.Generic.Dictionary[string,int]' (
    [StringComparer]::Ordinal)    # name -> count (raw, any context)
$strictHits = New-Object 'System.Collections.Generic.Dictionary[string,int]' (
    [StringComparer]::Ordinal)    # name -> count (filtered)
$perRepo = New-Object 'System.Collections.Generic.Dictionary[string,object]' (
    [StringComparer]::Ordinal)    # name -> @{dataset;sample;default}
foreach ($n in $names) {
    $fullHits[$n] = 0
    $strictHits[$n] = 0
    $perRepo[$n] = @{ dataset = 0; sample = 0; default = 0 }
}
$strictSites = New-Object 'System.Collections.Generic.List[string]'
# one hit per name per line: a line is a call site, not every occurrence
$seenFull = New-Object 'System.Collections.Generic.HashSet[string]' (
    [StringComparer]::Ordinal)
$seenStrict = New-Object 'System.Collections.Generic.HashSet[string]' (
    [StringComparer]::Ordinal)

$sw = [Diagnostics.Stopwatch]::StartNew()
$fileCount = 0
$lineCount = 0
foreach ($root in $roots) {
    if (-not (Test-Path $root.path)) {
        Write-Host ('WARN: missing scan root ' + $root.path)
        continue
    }
    $files = Get-ChildItem $root.path -Recurse -File -Include *.lua |
        Sort-Object FullName
    foreach ($f in $files) {
        $fileCount++
        $rel = $f.FullName
        $lines = [IO.File]::ReadAllLines($f.FullName)
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $line = $lines[$i]
            $lineCount++
            if (-not $line.Contains('(')) { continue }
            $isComment = $commentRx.IsMatch($line)
            $defSpans = if ($isComment) { $null } else { $defRx.Matches($line) }
            [void]$seenFull.Clear()
            [void]$seenStrict.Clear()
            foreach ($m in $callRx.Matches($line)) {
                $nm = $m.Groups['id'].Value
                if (-not $nameSet.Contains($nm)) { continue }
                if ($seenFull.Add($nm)) { $fullHits[$nm]++ }
                if ($isComment) { continue }
                if ($m.Index -gt 0 -and $line[$m.Index - 1] -eq '.') { continue }
                $inDef = $false
                foreach ($d in $defSpans) {
                    if ($m.Index -ge $d.Index -and
                        $m.Index -lt ($d.Index + $d.Length)) {
                        $inDef = $true
                        break
                    }
                }
                if ($inDef) { continue }
                if (-not $seenStrict.Add($nm)) { continue }
                $strictHits[$nm]++
                $perRepo[$nm][$root.key]++
                $strictSites.Add(('{0},{1},{2}' -f $nm, $rel, ($i + 1)))
            }
        }
    }
}

# -------------------------------------------------------------- reports ----
$zero = @($names | Where-Object { $strictHits[$_] -eq 0 } | Sort-Object)
$le1 = @($names | Where-Object { $strictHits[$_] -le 1 }).Count
$le3 = @($names | Where-Object { $strictHits[$_] -le 3 }).Count
$le10 = @($names | Where-Object { $strictHits[$_] -le 10 }).Count

# per-function buckets: a function's usage is the sum over its Lua names
$funcHits = @{}
$seenOnce = New-Object 'System.Collections.Generic.HashSet[string]' (
    [StringComparer]::Ordinal)
foreach ($e in $entries) {
    if (-not $funcHits.ContainsKey($e.func)) { $funcHits[$e.func] = 0 }
    if ($seenOnce.Add($e.name)) { $funcHits[$e.func] += $strictHits[$e.name] }
}
$fZero = @($funcHits.Keys | Where-Object { $funcHits[$_] -eq 0 }).Count
$fLe1 = @($funcHits.Keys | Where-Object { $funcHits[$_] -le 1 }).Count
$fLe3 = @($funcHits.Keys | Where-Object { $funcHits[$_] -le 3 }).Count
$fLe10 = @($funcHits.Keys | Where-Object { $funcHits[$_] -le 10 }).Count

Write-Host ''
Write-Host ('=== Usage scan: {0} files / {1} lines in {2:n1}s ===' -f
    $fileCount, $lineCount, $sw.Elapsed.TotalSeconds)
Write-Host ('0-call names : {0}' -f $zero.Count)
Write-Host ('  ' + ($zero -join ', '))
Write-Host ('buckets over {0} names: 0={1} <=1={2} <=3={3} <=10={4}' -f
    $names.Count, $zero.Count, $le1, $le3, $le10)
Write-Host ('buckets over {0} funcs: 0={1} <=1={2} <=3={3} <=10={4}' -f
    $funcs.Count, $fZero, $fLe1, $fLe3, $fLe10)

$usagePath = Join-Path $outDir 'bind_usage.txt'
$ub = New-Object 'System.Collections.Generic.List[string]'
$ub.Add('# strict usage of registered Lua names (see check-binds.ps1 header)')
$ub.Add(('entries={0} names={1} funcs={2}' -f $entries.Count,
    $names.Count, $funcs.Count))
$ub.Add(('0-call={0} <=1={1} <=3={2} <=10={3} (names)' -f $zero.Count, $le1,
    $le3, $le10))
$ub.Add(('0-call={0} <=1={1} <=3={2} <=10={3} (funcs)' -f $fZero, $fLe1,
    $fLe3, $fLe10))
$ub.Add('name<tab>strict<tab>full<tab>func<tab>tables')
foreach ($n in ($names | Sort-Object { $strictHits[$_] }, { $_ })) {
    $ents = @($entries | Where-Object { $_.name -ceq $n })
    $tbls = @($ents | ForEach-Object table) -join '+'
    $fn = $ents[0].func
    $ub.Add(('{0}`t{1}`t{2}`t{3}`t{4}' -f $n, $strictHits[$n],
        $fullHits[$n], $fn, $tbls))
}
[IO.File]::WriteAllLines($usagePath, $ub)

$fullCsv = Join-Path $outDir 'bind_full.csv'
$rows = New-Object 'System.Collections.Generic.List[string]'
$rows.Add('name,tables,func,impl,full_hits,strict_hits')
foreach ($n in ($names | Sort-Object)) {
    $ents = @($entries | Where-Object { $_.name -ceq $n })
    $fn = $ents[0].func
    $tbls = ($ents | ForEach-Object table) -join '+'
    $impl = if ($impls.ContainsKey($fn)) { $impls[$fn] } else { '' }
    $rows.Add(('{0},{1},{2},{3},{4},{5}' -f $n, $tbls, $fn, $impl,
        $fullHits[$n], $strictHits[$n]))
}
[IO.File]::WriteAllLines($fullCsv, $rows)

$strictCsv = Join-Path $outDir 'bind_strict.csv'
$sites = New-Object 'System.Collections.Generic.List[string]'
$sites.Add('name,file,line')
foreach ($s in $strictSites) { $sites.Add($s) }
[IO.File]::WriteAllLines($strictCsv, $sites)

$usage2Csv = Join-Path $outDir 'bind_usage2.csv'
$u2 = New-Object 'System.Collections.Generic.List[string]'
$u2.Add('name,dataset,sample,default,total')
foreach ($n in ($names | Sort-Object)) {
    $p = $perRepo[$n]
    $u2.Add(('{0},{1},{2},{3},{4}' -f $n, $p.dataset, $p.sample, $p.default,
        $strictHits[$n]))
}
[IO.File]::WriteAllLines($usage2Csv, $u2)

Write-Host ''
Write-Host ('wrote: {0}' -f $usagePath)
Write-Host ('wrote: {0}' -f $fullCsv)
Write-Host ('wrote: {0}' -f $strictCsv)
Write-Host ('wrote: {0}' -f $usage2Csv)

exit $(if ($problems.Count -gt 0) { 1 } else { 0 })

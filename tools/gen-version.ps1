# Generate src/config/VersionAuto.h to bake the git commit into MAIN_VERSION.
# Called by the pre-build target of jianghu.vcxproj / jianghu_core.vcxproj.
#
# Version format: GS<commit date YYYYMMDD>-<short commit>, e.g. GS20251028-6514d63.
# - Commit date (not build date) is used so rebuilding the same commit is reproducible.
# - Falls back to GS<build date>-nogit when git is unavailable / not a git repo.
# - Writes the file only when content changes, to avoid needless recompiles.
# This script always exits 0 and never fails the build.
# NOTE: keep this file ASCII-only so Windows PowerShell reads it regardless of codepage.

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$out      = Join-Path $repoRoot "src\config\VersionAuto.h"

$hash = "nogit"
$date = Get-Date -Format "yyyyMMdd"

try {
    $g = git -C $repoRoot rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $g) { $hash = "$g".Trim() }

    $d = git -C $repoRoot log -1 --format=%cd --date=format:%Y%m%d 2>$null
    if ($LASTEXITCODE -eq 0 -and $d) { $date = "$d".Trim() }
} catch {
    # keep fallback values
}

$version = "GS$date-$hash"
$content = "#pragma once`r`n" +
           "// AUTO-GENERATED: do not edit. Produced by tools/gen-version.ps1 at build time.`r`n" +
           "#define MAIN_VERSION `"$version`"`r`n"

$old = $null
if (Test-Path $out) {
    try { $old = [IO.File]::ReadAllText($out) } catch { $old = $null }
}

if ($old -ne $content) {
    $dir = Split-Path -Parent $out
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [IO.File]::WriteAllText($out, $content)  # default UTF-8 without BOM
}

exit 0

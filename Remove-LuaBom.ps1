# .\Remove-LuaBom.ps1 -Path "c:\jianghu-engine\lua-sample\"

param(
    [string]$Path = ".",
    [string]$Filter = "*.lua",
    [switch]$Recurse = $true,
    [switch]$WhatIfOnly
)

$bom = [byte[]](0xEF, 0xBB, 0xBF)
$changedCount = 0

$files = if ($Recurse) {
    Get-ChildItem -Path $Path -Filter $Filter -File -Recurse
} else {
    Get-ChildItem -Path $Path -Filter $Filter -File
}

foreach ($file in $files) {
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)

    if ($bytes.Length -lt 3) { continue }
    if (-not ($bytes[0] -eq $bom[0] -and $bytes[1] -eq $bom[1] -and $bytes[2] -eq $bom[2])) {
        continue
    }

    Write-Host "Removing BOM: $($file.FullName)" -ForegroundColor Yellow

    if ($WhatIfOnly) { $changedCount++; continue }

    # Strip only the first 3 bytes, preserve everything else exactly
    $newBytes = New-Object byte[] ($bytes.Length - 3)
    [Array]::Copy($bytes, 3, $newBytes, 0, $newBytes.Length)
    [System.IO.File]::WriteAllBytes($file.FullName, $newBytes)
    $changedCount++
}

Write-Host ""
Write-Host "Done. $changedCount file(s) processed." -ForegroundColor Green

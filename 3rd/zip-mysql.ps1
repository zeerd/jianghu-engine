# .\zip-mysql.ps1 3rd\x64\mysql-connector-c-6.1.11-winx64 3rd\x86\mysql-connector-c-6.1.11-win32 lib\libmysql.dll

param(
    [Parameter(Mandatory, Position = 0)][string]$DirA,
    [Parameter(Mandatory, Position = 1)][string]$DirB,
    [Parameter(Mandatory, Position = 2)][string]$RelPath,
    [Parameter(Position = 3)][string]$ZipPath,
    [string]$PrefixA = 'x64',
    [string]$PrefixB = 'x86',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not (Test-Path -LiteralPath $DirA -PathType Container)) { throw "目录不存在: $DirA" }
if (-not (Test-Path -LiteralPath $DirB -PathType Container)) { throw "目录不存在: $DirB" }

$rootA = (Resolve-Path -LiteralPath $DirA).ProviderPath.TrimEnd('\')
$rootB = (Resolve-Path -LiteralPath $DirB).ProviderPath.TrimEnd('\')

# 统一分隔符，去掉开头的 \
$rel = ($RelPath -replace '/', '\').TrimStart('\')

$fileA = Join-Path $rootA $rel
$fileB = Join-Path $rootB $rel

if (-not (Test-Path -LiteralPath $fileA -PathType Leaf)) { throw "a 中不存在: $fileA" }
if (-not (Test-Path -LiteralPath $fileB -PathType Leaf)) { throw "b 中不存在: $fileB" }

# 未指定输出时，默认放到当前目录，文件名取相对路径的文件名 + .zip
if (-not $ZipPath) {
    $ZipPath = Join-Path (Get-Location).ProviderPath (([IO.Path]::GetFileName($rel)) + '.zip')
}
$ZipPath = [IO.Path]::GetFullPath($ZipPath)

if (Test-Path -LiteralPath $ZipPath) {
    if (-not $Force) { throw "目标已存在: $ZipPath（加 -Force 覆盖）" }
    Remove-Item -LiteralPath $ZipPath -Force
}

$entryRel = $rel -replace '\\', '/'
$zip = [System.IO.Compression.ZipFile]::Open(
    $ZipPath,
    [System.IO.Compression.ZipArchiveMode]::Create
)
try {
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip, $fileA, "$PrefixA/$entryRel") | Out-Null
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip, $fileB, "$PrefixB/$entryRel") | Out-Null
}
finally {
    $zip.Dispose()
}

Write-Host "已生成: $ZipPath"
Write-Host "  $PrefixA/$entryRel"
Write-Host "  $PrefixB/$entryRel"

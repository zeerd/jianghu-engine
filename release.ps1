# release.ps1 — 发布入口：全量重建 Release|x64 → 组装发行包 → 可选发 GitHub Release
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File release.ps1                  # 只构建 + 打包
#   powershell -ExecutionPolicy Bypass -File release.ps1 release v1.2.0   # 构建 + 打包 + gh 发布
#   powershell -ExecutionPolicy Bypass -File release.ps1 -Tag v1.2.0      # 同上（命名参数写法）
#
# 发行包：dist\jianghu-<版本>-x64.zip，解开为同名目录，内容为
#   jianghu.exe、libmysql.dll、config.ini、lua-default\、lua-sample\
# 调试符号（pdb/map）与测试可执行文件不进发行包。
#
# config.ini 由 config.ini.sample 生成，[lua] 的两处脚本根改写成随包相对目录：
# 服务器按 CWD 解析相对路径，在包目录起服即可加载脚本；仓库根的 sample 保留
# 开发机的绝对路径，本脚本不改动它。

param(
    [Parameter(Position = 0)]
    [string]$Release,

    [Parameter(Position = 1)]
    [string]$Tag
)

$ErrorActionPreference = 'Stop'
$RootDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# ===========================================================================
# 参数：首个参数是 'release' 时进入发布；tag 由其后参数给出（-Tag 亦可）
# ===========================================================================
if ($Release -and $Release -ine 'release') {
    throw "首个参数只认 'release'（其后跟 tag），收到：$Release"
}
if ($Release -ieq 'release' -and -not $Tag) {
    throw "release 之后必须跟 tag，例如：release.ps1 release v1.2.0"
}
$DoPublish = [bool]$Tag   # 有 tag 即发布（'release <tag>' 或 -Tag <tag>）

# 版本名：发布用 tag，否则退回 git 描述 / 时间戳
$Version = $Tag
if (-not $Version) {
    $ErrorActionPreference = 'Continue'
    $Version = (git -C $RootDir describe --tags --always 2>$null)
    $ErrorActionPreference = 'Stop'
    if (-not $Version) { $Version = 'dev-' + (Get-Date -Format 'yyyyMMdd-HHmm') }
}
$Version = ($Version -replace '[\\/:*?"<>|]', '-').Trim()
$DistDir = Join-Path $RootDir 'dist'
$StageDir = Join-Path $DistDir "jianghu-$Version"
$ZipPath = Join-Path $DistDir "jianghu-$Version-x64.zip"

# ===========================================================================
# 1. 构建 Release|x64
# ===========================================================================
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

$OutDir = Join-Path $RootDir 'x64\Release'

Write-Host "=== 全量重建 Release|x64 ===" -ForegroundColor Cyan
$msbuild = Get-MSBuildPath
Write-Host "MSBuild: $msbuild" -ForegroundColor DarkGray
# 发行包必须来自当前源码的完整编译，因此走 /t:Rebuild（Clean + 全量编译），
# 不走增量 Build：增量产物可能沿用旧对象文件与旧头文件。
# 先重建被引用的 jianghu_core 再重建 jianghu——命令行下 ProjectReference 只对
# 引用方调 Build，指望一次 Rebuild 连带清掉核心库是不可靠的。
# SolutionDir 显式给出：直接构建 vcxproj 时该属性为空，输出目录要与从解决方案
# 构建时一致（$(SolutionDir)x64\$(Configuration)\），不能落到相对路径上。
foreach ($proj in @('jianghu_core.vcxproj', 'jianghu.vcxproj')) {
    & $msbuild (Join-Path $RootDir $proj) /t:Rebuild /p:Configuration=Release /p:Platform=x64 `
        "/p:SolutionDir=$RootDir\" /m /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "全量重建失败（$proj Release|x64）" }
}

$ExePath = Join-Path $OutDir 'jianghu.exe'
$DllPath = Join-Path $OutDir 'libmysql.dll'
if (-not (Test-Path $ExePath)) { throw "构建产物缺失：$ExePath" }
# PostBuildEvent 只在目标不存在时补 DLL，直接构建时兜底复制一份
if (-not (Test-Path $DllPath)) {
    $srcDll = Join-Path $RootDir '3rd\x64\mysql-connector-c-6.1.11-winx64\lib\libmysql.dll'
    if (-not (Test-Path $srcDll)) { throw "运行期 DLL 缺失：$DllPath" }
    Copy-Item $srcDll $DllPath
}
Write-Host ("构建完成：{0}（{1}）" -f $ExePath, (Get-Item $ExePath).LastWriteTime) -ForegroundColor Green

# ===========================================================================
# 2. 组装发行包
# ===========================================================================
Write-Host "`n=== 组装发行包 $StageDir ===" -ForegroundColor Cyan
if (Test-Path $StageDir) { Remove-Item $StageDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

Copy-Item $ExePath $StageDir
Copy-Item $DllPath $StageDir

# 配置：sample → 包内 config.ini，脚本根改随包相对目录（服务器按 CWD 解析）
$samplePath = Join-Path $RootDir 'config.ini.sample'
if (-not (Test-Path $samplePath)) { throw "配置模板缺失：$samplePath" }
$cfg = [System.IO.File]::ReadAllText($samplePath)
$cfg = $cfg -replace '(?m)^(\s*script_root\s*=\s*).*$', '${1}lua-sample'
$cfg = $cfg -replace '(?m)^(\s*default_script_root\s*=\s*).*$', '${1}lua-default'
[System.IO.File]::WriteAllText((Join-Path $StageDir 'config.ini'), $cfg, (New-Object Text.UTF8Encoding($false)))

Copy-Item (Join-Path $RootDir 'lua-default') -Destination $StageDir -Recurse
Copy-Item (Join-Path $RootDir 'lua-sample') -Destination $StageDir -Recurse

$exeCount = (Get-ChildItem $StageDir -Recurse -File | Measure-Object).Count
Write-Host ("发行目录就绪：{0} 个文件" -f $exeCount) -ForegroundColor Green

if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $StageDir -DestinationPath $ZipPath
if (-not (Test-Path $ZipPath)) { throw "打包失败：$ZipPath" }
Write-Host ("发行包：{0}（{1:N1} MB）" -f $ZipPath, ((Get-Item $ZipPath).Length / 1MB)) -ForegroundColor Green

# ===========================================================================
# 3. 发布（-Tag / release <tag> 时）
# ===========================================================================
if (-not $DoPublish) {
    Write-Host "`n未指定 tag，跳过发布。" -ForegroundColor DarkGray
    exit 0
}

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "找不到 gh（GitHub CLI），无法发布"
}

Write-Host "`n=== 发布 $Tag ===" -ForegroundColor Cyan
Push-Location $RootDir
try {
    # gh 从当前目录推断仓库；stderr 重定向在 Stop 下会抛错，先放宽再恢复
    $ErrorActionPreference = 'Continue'
    $null = gh auth status 2>$null
    if ($LASTEXITCODE -ne 0) { throw "gh 未登录，请先执行：gh auth login" }

    $null = gh release view $Tag 2>$null
    if ($LASTEXITCODE -eq 0) {
        # release 已存在：只换附件，标题与说明保持原样
        gh release upload $Tag $ZipPath --clobber
    } else {
        gh release create $Tag $ZipPath --title $Tag --generate-notes
    }
    $rc = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($rc -ne 0) { throw "发布失败（tag=$Tag）" }
} finally {
    Pop-Location
}

Write-Host "`n发布完成：$Tag" -ForegroundColor Green

# scripts/release.ps1
# 一键构建并打包自包含的 Windows 发布包.
#
# 流程:
#   1. cmake -S . -B build             (增量配置, 第一次会拉 cpr/curl/zlib/json/cli11)
#   2. cmake --build build -j          (Release 编译)
#   3. 从 build/ 顶层复制 translate.exe + 3 个 dll 到 dist/bin/
#      (不调用 cmake --install, 因为 FetchContent 拉下来的 zlib 子项目
#       把 install 路径硬编码成了 C:/Program Files (x86)/TranslateCLI,
#       在 cmake --install 时会触发权限错误.)
#   4. Compress-Archive                (打成 translate-cli-windows-x64.zip)
#
# 用法 (PowerShell, 在项目根目录):
#   .\scripts\release.ps1
#
# 自定义参数:
#   -Generator  "MinGW Makefiles" | "Ninja" | "Visual Studio 17 2022"  (默认: 自动)
#   -BuildType  Release | RelWithDebInfo                             (默认: Release)
#   -SkipBuild  跳过步骤 1+2, 直接对现有 build/ 做打包
#   -Output     zip 输出路径                                          (默认: 项目根)

[CmdletBinding()]
param(
    [string]$Generator = "",
    [string]$BuildType = "Release",
    [switch]$SkipBuild,
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $ProjectRoot

# 1+2. 配置 + 编译
if (-not $SkipBuild) {
    $cmakeArgs = @("-S", ".", "-B", "build", "-DCMAKE_BUILD_TYPE=$BuildType")
    if ($Generator) { $cmakeArgs += @("-G", $Generator) }
    Write-Host ">>> cmake 配置: cmake $($cmakeArgs -join ' ')" -ForegroundColor Cyan
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "cmake 配置失败 (exit $LASTEXITCODE)" }

    Write-Host ">>> cmake 编译: cmake --build build -j" -ForegroundColor Cyan
    & cmake --build build --config $BuildType -j
    if ($LASTEXITCODE -ne 0) { throw "cmake 编译失败 (exit $LASTEXITCODE)" }
}

# 3. 清理旧 dist, 准备 dist/bin/
$dist = Join-Path $ProjectRoot "dist"
$bin = Join-Path $dist "bin"
if (Test-Path $dist) {
    Write-Host ">>> 清理旧目录: $dist" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $dist
}
New-Item -ItemType Directory -Path $bin -Force | Out-Null

# 复制 translate.exe + 3 个 dll
$required = @("translate.exe", "libcpr.dll", "libcurl.dll", "libzlib.dll")
foreach ($name in $required) {
    $src = Join-Path "build" $name
    if (-not (Test-Path $src)) {
        throw "缺少构建产物: $src`n请确认 cmake --build 已成功完成"
    }
    Copy-Item $src -Destination $bin
}

# 4. 打包 zip
if (-not $Output) {
    $Output = Join-Path $ProjectRoot "translate-cli-windows-x64.zip"
}
if (Test-Path $Output) { Remove-Item -Force $Output }

Write-Host ">>> 打包: $Output" -ForegroundColor Cyan
Compress-Archive -Path (Join-Path $dist "*") -DestinationPath $Output

Write-Host ""
Write-Host "==== 发布包构建完成 ====" -ForegroundColor Green
Write-Host "目录: $bin"
Get-ChildItem $bin | Format-Table Name, Length -AutoSize
Write-Host "压缩包: $Output"

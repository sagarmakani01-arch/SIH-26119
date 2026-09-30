param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [switch]$Cuda,
    [switch]$Tests
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root $BuildDir

$Scripts = & python -c "import sysconfig; print(sysconfig.get_path('scripts'))"
$CMake = Join-Path $Scripts "cmake.exe"
$Ninja = Join-Path $Scripts "ninja.exe"
if (-not (Test-Path $CMake)) { throw "cmake not found at $CMake" }

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $VsPath) { throw "MSVC toolchain not found" }
$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"

$BuildTests = if ($Tests) { "ON" } else { "ON" }
$CudaFlag = if ($Cuda) { "ON" } else { "OFF" }

$Configure = "`"$CMake`" -S `"$Root`" -B `"$Build`" -G Ninja -DCMAKE_BUILD_TYPE=$Config -DSOLVER_ENABLE_CUDA=$CudaFlag -DSOLVER_BUILD_TESTS=$BuildTests"
$BuildCmd = "`"$CMake`" --build `"$Build`" --parallel"
$Command = "`"$VcVars`" >nul 2>&1 && $Configure && $BuildCmd"

Write-Output "Configuring and building in $Build ($Config, CUDA=$CudaFlag)"
cmd /c $Command
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
Write-Output "Build succeeded: $Build"

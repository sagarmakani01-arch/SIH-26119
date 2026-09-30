param([string]$BuildDir = "build", [string]$Filter = "")

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root $BuildDir

$Scripts = & python -c "import sysconfig; print(sysconfig.get_path('scripts'))"
$CMake = Join-Path $Scripts "cmake.exe"

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"

$CTestArgs = "--output-on-failure"
if ($Filter) { $CTestArgs = "-R `"$Filter`" $CTestArgs" }
$Command = "`"$VcVars`" >nul 2>&1 && `"$CMake`" --build `"$Build`" --parallel && cd /d `"$Build`" && `"$CMake`" -E env ctest.exe $CTestArgs"

cmd /c $Command
exit $LASTEXITCODE

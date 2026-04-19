[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [string]$ScriptPath = "tests/smoke/scripts/main_menu_to_site_active.smoke",
    [switch]$BuildFirst,
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot
$smokeExePath = Join-Path $buildPath "$Configuration\gs1_smoke_host.exe"
$dllPath = Join-Path $buildPath "$Configuration\gs1_game.dll"
$resolvedScriptPath = Resolve-RepoPath -Path $ScriptPath -RepoRoot $repoRoot
$hostVerbose = $VerbosePreference -ne 'SilentlyContinue'

if ($BuildFirst) {
    & (Join-Path $PSScriptRoot "build_gameplay_dll.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Gameplay DLL build script failed."
    }

    & (Join-Path $PSScriptRoot "build_smoke_host.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Smoke host build script failed."
    }
}

if (!(Test-Path $smokeExePath)) {
    throw "Smoke executable not found: $smokeExePath"
}

if (!(Test-Path $dllPath)) {
    throw "Gameplay DLL not found: $dllPath"
}

if (!(Test-Path $resolvedScriptPath)) {
    throw "Smoke script not found: $resolvedScriptPath"
}

$arguments = @($dllPath, $resolvedScriptPath)
if ($hostVerbose) {
    $arguments += "--verbose"
}

Write-Host ">> $smokeExePath $($arguments -join ' ')"
& $smokeExePath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Smoke test failed with exit code $LASTEXITCODE."
}

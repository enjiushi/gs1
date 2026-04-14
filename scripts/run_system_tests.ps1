[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [string[]]$System = @(),
    [string[]]$Asset = @(),
    [string]$AssetDir = "tests/system/assets",
    [switch]$BuildFirst,
    [switch]$List,
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot
$hostExePath = Join-Path $buildPath "$Configuration\gs1_system_test_host.exe"
$dllPath = Join-Path $buildPath "$Configuration\gs1_game.dll"
$resolvedAssetDir = Resolve-RepoPath -Path $AssetDir -RepoRoot $repoRoot

if ($BuildFirst) {
    & (Join-Path $PSScriptRoot "build_system_test_host.ps1") `
        -Configuration $Configuration `
        -BuildDir $BuildDir `
        -System $System `
        -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "System test host build script failed."
    }
}

if (!(Test-Path $hostExePath)) {
    throw "System test host executable not found: $hostExePath"
}

if (!(Test-Path $dllPath)) {
    throw "Gameplay DLL not found: $dllPath"
}

$arguments = @($dllPath, "--asset-dir", $resolvedAssetDir)

if ($List) {
    $arguments += "--list"
}
elseif ($System.Count -eq 0 -and $Asset.Count -eq 0) {
    $arguments += "--all"
}

foreach ($systemName in $System) {
    $arguments += @("--system", $systemName)
}

foreach ($assetPath in $Asset) {
    $resolvedAssetPath = Resolve-RepoPath -Path $assetPath -RepoRoot $repoRoot
    $arguments += @("--asset", $resolvedAssetPath)
}

Write-Host ">> $hostExePath $($arguments -join ' ')"
& $hostExePath @arguments
if ($LASTEXITCODE -ne 0) {
    throw "System tests failed with exit code $LASTEXITCODE."
}

[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot

Invoke-CMake -CMakePath $CMakePath -Arguments @(
    "-S", $repoRoot,
    "-B", $buildPath,
    "-DGS1_BUILD_TESTS=ON"
)

Invoke-CMake -CMakePath $CMakePath -Arguments @(
    "--build", $buildPath,
    "--config", $Configuration,
    "--target", "gs1_game_process_test_host"
)

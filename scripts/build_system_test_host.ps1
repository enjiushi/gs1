[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [string[]]$System = @(),
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot
$systemFilter = if ($System.Count -gt 0) { $System -join ";" } else { "" }

Invoke-CMake -CMakePath $CMakePath -Arguments @(
    "-S", $repoRoot,
    "-B", $buildPath,
    "-DGS1_BUILD_TESTS=ON",
    "-DGS1_SYSTEM_TEST_SYSTEMS=$systemFilter"
)

Invoke-CMake -CMakePath $CMakePath -Arguments @(
    "--build", $buildPath,
    "--config", $Configuration,
    "--target", "gs1_system_test_host"
)

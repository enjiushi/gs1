[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [string[]]$Scenario = @(),
    [switch]$BuildFirst,
    [switch]$List,
    [uint32]$MaxFrames = 18000,
    [double]$FrameDeltaSeconds = 0.25,
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot
$hostExePath = Join-Path $buildPath "$Configuration\gs1_game_process_test_host.exe"
$dllPath = Join-Path $buildPath "$Configuration\gs1_game.dll"
$hostVerbose = $VerbosePreference -ne 'SilentlyContinue'
$frameDeltaText = $FrameDeltaSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)

if ($BuildFirst) {
    & (Join-Path $PSScriptRoot "build_gameplay_dll.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Gameplay DLL build script failed."
    }

    & (Join-Path $PSScriptRoot "build_process_test_host.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Process test host build script failed."
    }
}

if (!(Test-Path $hostExePath)) {
    throw "Process test host executable not found: $hostExePath"
}

if (!$List -and !(Test-Path $dllPath)) {
    throw "Gameplay DLL not found: $dllPath"
}

$arguments = @()
if (-not $List) {
    $arguments += $dllPath
}

if ($List) {
    $arguments += "--list"
} elseif ($Scenario.Count -eq 0) {
    $arguments += "--all"
}

foreach ($scenarioName in $Scenario) {
    $arguments += @("--scenario", $scenarioName)
}

$arguments += @("--max-frames", "$MaxFrames", "--frame-delta-seconds", $frameDeltaText)

if ($hostVerbose) {
    $arguments += "--verbose"
}

Write-Host ">> $hostExePath $($arguments -join ' ')"
Push-Location $repoRoot
try {
    & $hostExePath @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Process tests failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

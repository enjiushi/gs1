[CmdletBinding()]
param(
    [string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [uint16]$Port = 18765,
    [switch]$BuildFirst,
    [switch]$NoBrowser,
    [string]$LogPath = "out/logs/visual_smoke_host_latest.log",
    [string]$CMakePath
)

. (Join-Path $PSScriptRoot "common.ps1")

$repoRoot = Get-RepoRoot
$buildPath = Resolve-RepoPath -Path $BuildDir -RepoRoot $repoRoot
$visualExePath = Join-Path $buildPath "$Configuration\gs1_visual_smoke_host.exe"
$dllPath = Join-Path $buildPath "$Configuration\gs1_game.dll"
$resolvedLogPath = Resolve-RepoPath -Path $LogPath -RepoRoot $repoRoot

if ($BuildFirst) {
    & (Join-Path $PSScriptRoot "build_gameplay_dll.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Gameplay DLL build script failed."
    }

    & (Join-Path $PSScriptRoot "build_visual_smoke_host.ps1") -Configuration $Configuration -BuildDir $BuildDir -CMakePath $CMakePath
    if ($LASTEXITCODE -ne 0) {
        throw "Visual smoke host build script failed."
    }
}

if (!(Test-Path $visualExePath)) {
    throw "Visual smoke executable not found: $visualExePath"
}

if (!(Test-Path $dllPath)) {
    throw "Gameplay DLL not found: $dllPath"
}

$logDirectory = Split-Path -Parent $resolvedLogPath
if ($logDirectory -and !(Test-Path $logDirectory)) {
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
}

if (Test-Path $resolvedLogPath) {
    Remove-Item -LiteralPath $resolvedLogPath -Force
}

# Always start from a fresh runtime so the adapter boots back into MAIN_MENU.
$existingHosts = Get-Process -Name "gs1_visual_smoke_host" -ErrorAction SilentlyContinue
if ($null -ne $existingHosts) {
    $existingHosts | Stop-Process -Force
    Start-Sleep -Milliseconds 400
}

$arguments = @($dllPath)
if ($Port -gt 0) {
    $arguments += @("--port", "$Port")
}

$escapedRepoRoot = $repoRoot -replace "'", "''"
$escapedExePath = $visualExePath -replace "'", "''"
$escapedDllPath = $dllPath -replace "'", "''"
$launchCommand = "Set-Location '$escapedRepoRoot'; & '$escapedExePath' '$escapedDllPath'"
if ($Port -gt 0) {
    $launchCommand += " --port $Port"
}

Write-Host ">> powershell.exe -Command $launchCommand"
$process = Start-Process `
    -FilePath "powershell.exe" `
    -ArgumentList @("-Command", $launchCommand) `
    -WorkingDirectory $repoRoot `
    -PassThru

if ($Port -gt 0) {
    $healthUrl = "http://127.0.0.1:$Port/health"
    $isReady = $false

    for ($attempt = 0; $attempt -lt 30; $attempt += 1) {
        Start-Sleep -Milliseconds 200

        try {
            $healthResponse = Invoke-WebRequest -UseBasicParsing $healthUrl
            if ($healthResponse.StatusCode -eq 200) {
                $isReady = $true
                break
            }
        }
        catch {
        }
    }

    if (-not $isReady) {
        throw "Visual smoke host did not become ready at $healthUrl"
    }

    if (-not $NoBrowser) {
        Start-Process "http://127.0.0.1:$Port/" | Out-Null
    }
}

Write-Host "Visual smoke host started with PID $($process.Id)."
Write-Host "Latest visual host log: $resolvedLogPath"
if ($Port -gt 0) {
    Write-Host "Open http://127.0.0.1:$Port/ in your browser."
} else {
    Write-Host "The host chooses an available localhost port and prints it in its own console window."
}

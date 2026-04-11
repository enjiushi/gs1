[CmdletBinding()]
param()

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-RepoPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return (Join-Path $RepoRoot $Path)
}

function Resolve-CMakePath {
    param(
        [string]$CMakePath
    )

    if ($CMakePath -and (Test-Path $CMakePath)) {
        return (Resolve-Path $CMakePath).Path
    }

    $defaultPath = "E:\tools\cmake\bin\cmake.exe"
    if (Test-Path $defaultPath) {
        return $defaultPath
    }

    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Unable to find cmake.exe. Pass -CMakePath explicitly."
}

function Invoke-CMake {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$CMakePath
    )

    $resolvedCMakePath = Resolve-CMakePath -CMakePath $CMakePath
    Write-Host ">> $resolvedCMakePath $($Arguments -join ' ')"
    & $resolvedCMakePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake command failed with exit code $LASTEXITCODE."
    }
}

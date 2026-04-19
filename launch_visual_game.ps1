[CmdletBinding()]
param(
    [switch]$BuildFirst,
    [uint16]$Port = 18765
)

& (Join-Path $PSScriptRoot "scripts\run_visual_smoke.ps1") `
    -Configuration "Debug" `
    -BuildDir "build" `
    -Port $Port `
    -BuildFirst:$BuildFirst `
    -Verbose:$($VerbosePreference -ne 'SilentlyContinue')

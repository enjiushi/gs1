[CmdletBinding()]
param(
    [string]$ConfigPath,
    [string]$Vendor,
    [string]$SnapshotId,
    [switch]$Latest,
    [string[]]$EntryName,
    [string]$GsUtilPath,
    [string]$RClonePath,
    [switch]$DryRun,
    [switch]$SkipDelete
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "gcs_common.ps1")

$configPathToUse = Get-GcsContentConfigPath -ConfigPath $ConfigPath
if ($PSBoundParameters.ContainsKey('Vendor')) {
    $normalizedVendor = Set-GcsContentVendor -Vendor $Vendor
    $configDocument = Read-GcsContentConfigDocument -ConfigPath $configPathToUse
    $configDocument.vendor = $normalizedVendor
    Write-GcsContentConfigDocument -ConfigPath $configPathToUse -ConfigDocument $configDocument
}

$config = Get-GcsContentConfig -ConfigPath $configPathToUse
$contentRef = Resolve-GcsContentRef -Config $config -SnapshotId $SnapshotId -Latest:$Latest
$selectedEntries = Get-GcsSelectedEntries -Config $config -EntryName $EntryName

foreach ($entry in $selectedEntries) {
    $localPath = Get-GcsLocalPath -RepoRoot $config.RepoRoot -Entry $entry
    $remoteUri = Get-GcsRemoteUri -BucketUri $config.BucketUri -ContentRef $contentRef -Entry $entry
    Invoke-GcsTransfer -Direction pull -Vendor $config.Vendor -Entry $entry -LocalPath $localPath -RemoteUri $remoteUri -GsUtilPath $GsUtilPath -RClonePath $RClonePath -DryRun:$DryRun -SkipDelete:$SkipDelete
}

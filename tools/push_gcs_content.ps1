[CmdletBinding()]
param(
    [string]$ConfigPath,
    [string]$Vendor,
    [string]$SnapshotId,
    [switch]$Latest,
    [string[]]$EntryName,
    [string]$BucketUri,
    [string]$LatestLabel,
    [string]$GsUtilPath,
    [string]$GCloudPath,
    [string]$RClonePath,
    [switch]$Setup,
    [switch]$AuthLogin,
    [switch]$AuthApplicationDefault,
    [switch]$DryRun,
    [switch]$SkipDelete
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "gcs_common.ps1")

$configPathToUse = Get-GcsContentConfigPath -ConfigPath $ConfigPath

$normalizedVendor = $null
if ($PSBoundParameters.ContainsKey('Vendor')) {
    $normalizedVendor = Set-GcsContentVendor -Vendor $Vendor
}

if ($PSBoundParameters.ContainsKey('BucketUri') -or $PSBoundParameters.ContainsKey('LatestLabel') -or $null -ne $normalizedVendor) {
    $configDocument = Read-GcsContentConfigDocument -ConfigPath $configPathToUse
    if ($null -ne $normalizedVendor) {
        $configDocument.vendor = $normalizedVendor
    }

    if ($PSBoundParameters.ContainsKey('BucketUri')) {
        $configDocument.bucketUri = $BucketUri
    }

    if ($PSBoundParameters.ContainsKey('LatestLabel')) {
        $configDocument.latestLabel = $LatestLabel
    }

    Write-GcsContentConfigDocument -ConfigPath $configPathToUse -ConfigDocument $configDocument
    Write-Host "Updated GCS content config at '$configPathToUse'."
}

if ($Setup) {
    $setupConfig = Get-GcsContentConfig -ConfigPath $configPathToUse
    if ($setupConfig.Vendor -eq 'cloudflare') {
        Write-Host "Validating rclone for Cloudflare content upload."
        $null = Get-RClonePath -RClonePath $RClonePath
        Write-Host "Checking access to '$($setupConfig.BucketUri)' through rclone."
        Invoke-RClone -Arguments @('lsf', $setupConfig.BucketUri) -RClonePath $RClonePath
        Write-Host "Setup complete for Cloudflare. Use './tools/push_gcs_content.ps1 -Vendor cloudflare' to upload latest content or add -SnapshotId to publish a named snapshot."
        return
    }

    Write-Host "Validating Google Cloud SDK tools for GCS content upload."
    $null = Get-GCloudPath -GCloudPath $GCloudPath
    $null = Get-GsUtilPath -GsUtilPath $GsUtilPath

    if ($AuthLogin) {
        Invoke-GCloud -Arguments @('auth', 'login') -GCloudPath $GCloudPath
    }

    if ($AuthApplicationDefault) {
        Invoke-GCloud -Arguments @('auth', 'application-default', 'login') -GCloudPath $GCloudPath
    }

    Write-Host "Checking access to '$($setupConfig.BucketUri)'."
    Invoke-GsUtil -Arguments @('ls', $setupConfig.BucketUri) -GsUtilPath $GsUtilPath
    Write-Host "Setup complete for GCS. Use './tools/push_gcs_content.ps1' to upload latest content or add -SnapshotId to publish a named snapshot."
    return
}

$config = Get-GcsContentConfig -ConfigPath $configPathToUse
$contentRef = Resolve-GcsContentRef -Config $config -SnapshotId $SnapshotId -Latest:$Latest
$selectedEntries = Get-GcsSelectedEntries -Config $config -EntryName $EntryName

foreach ($entry in $selectedEntries) {
    $localPath = Get-GcsLocalPath -RepoRoot $config.RepoRoot -Entry $entry
    $remoteUri = Get-GcsRemoteUri -BucketUri $config.BucketUri -ContentRef $contentRef -Entry $entry
    Invoke-GcsTransfer -Direction push -Vendor $config.Vendor -Entry $entry -LocalPath $localPath -RemoteUri $remoteUri -GsUtilPath $GsUtilPath -RClonePath $RClonePath -DryRun:$DryRun -SkipDelete:$SkipDelete
}

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot "..\scripts\common.ps1")

function Get-GcsContentConfigPath {
    param(
        [string]$ConfigPath
    )

    $repoRoot = Get-RepoRoot
    if ($ConfigPath) {
        return Resolve-RepoPath -Path $ConfigPath -RepoRoot $repoRoot
    }

    return Join-Path $PSScriptRoot "gcs_content.json"
}

function Read-GcsContentConfigDocument {
    param(
        [string]$ConfigPath
    )

    $resolvedConfigPath = Get-GcsContentConfigPath -ConfigPath $ConfigPath
    if (-not (Test-Path $resolvedConfigPath)) {
        throw "Unable to find the GCS content config at '$resolvedConfigPath'."
    }

    return Get-Content $resolvedConfigPath -Raw | ConvertFrom-Json
}

function Write-GcsContentConfigDocument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ConfigPath,
        [Parameter(Mandatory = $true)]
        [object]$ConfigDocument
    )

    $json = $ConfigDocument | ConvertTo-Json -Depth 8
    [System.IO.File]::WriteAllText($ConfigPath, $json + [Environment]::NewLine)
}

function Update-GcsContentConfig {
    param(
        [string]$ConfigPath,
        [string]$BucketUri,
        [string]$LatestLabel
    )

    $resolvedConfigPath = Get-GcsContentConfigPath -ConfigPath $ConfigPath
    $config = Read-GcsContentConfigDocument -ConfigPath $resolvedConfigPath

    if ($PSBoundParameters.ContainsKey('BucketUri')) {
        $config.bucketUri = $BucketUri
    }

    if ($PSBoundParameters.ContainsKey('LatestLabel')) {
        $config.latestLabel = $LatestLabel
    }

    Write-GcsContentConfigDocument -ConfigPath $resolvedConfigPath -ConfigDocument $config
    return $resolvedConfigPath
}

function Get-GcsContentConfig {
    param(
        [string]$ConfigPath
    )

    $repoRoot = Get-RepoRoot
    $resolvedConfigPath = Get-GcsContentConfigPath -ConfigPath $ConfigPath
    $config = Read-GcsContentConfigDocument -ConfigPath $resolvedConfigPath
    $vendor = if ($env:GS1_STORAGE_VENDOR) { $env:GS1_STORAGE_VENDOR } else { [string]$config.vendor }
    $bucketUri = if ($env:GS1_GCS_BUCKET_URI) { $env:GS1_GCS_BUCKET_URI } else { [string]$config.bucketUri }
    $latestLabel = if ($env:GS1_GCS_LATEST_LABEL) { $env:GS1_GCS_LATEST_LABEL } else { [string]$config.latestLabel }

    if ([string]::IsNullOrWhiteSpace($vendor)) {
        $vendor = 'gcs'
    }

    $normalizedVendor = $vendor.Trim().ToLowerInvariant()
    if ($normalizedVendor -notin @('gcs', 'cloudflare')) {
        throw "Unsupported storage vendor '$vendor'. Use 'gcs' or 'cloudflare'."
    }

    if ([string]::IsNullOrWhiteSpace($bucketUri) -or $bucketUri -like '*replace-with-your-gs1-bucket*') {
        throw "Set 'bucketUri' in '$resolvedConfigPath' or the GS1_GCS_BUCKET_URI environment variable before syncing content."
    }

    return [pscustomobject]@{
        RepoRoot = $repoRoot
        ConfigPath = $resolvedConfigPath
        Vendor = $normalizedVendor
        BucketUri = $bucketUri.TrimEnd('/')
        LatestLabel = $latestLabel
        Entries = @($config.entries)
    }
}

function Set-GcsContentVendor {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Vendor
    )

    $normalizedVendor = $Vendor.Trim().ToLowerInvariant()
    if ($normalizedVendor -notin @('gcs', 'cloudflare')) {
        throw "Unsupported storage vendor '$Vendor'. Use 'gcs' or 'cloudflare'."
    }

    return $normalizedVendor
}

function Resolve-GcsContentRef {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Config,
        [string]$SnapshotId,
        [switch]$Latest
    )

    if ($Latest -and $SnapshotId) {
        throw "Use either -Latest or -SnapshotId, not both."
    }

    if ($SnapshotId) {
        return [pscustomobject]@{
            Kind = 'snapshot'
            Value = $SnapshotId.Trim()
        }
    }

    $resolvedLatestLabel = [string]$Config.LatestLabel
    if ([string]::IsNullOrWhiteSpace($resolvedLatestLabel)) {
        $resolvedLatestLabel = 'latest'
    }

    return [pscustomobject]@{
        Kind = 'latest'
        Value = $resolvedLatestLabel.Trim()
    }
}

function Get-GcsSelectedEntries {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Config,
        [string[]]$EntryName
    )

    $entries = @($Config.Entries)
    if ($EntryName -and $EntryName.Count -gt 0) {
        $lookup = @{}
        foreach ($name in $EntryName) {
            $lookup[$name] = $true
        }

        $entries = @($entries | Where-Object { $lookup.ContainsKey([string]$_.name) })
        $selectedNames = @{}
        foreach ($entry in $entries) {
            $selectedNames[[string]$entry.name] = $true
        }

        $missingEntries = @($EntryName | Where-Object { -not $selectedNames.ContainsKey($_) })
        if ($missingEntries.Count -gt 0) {
            throw "Unknown GCS content entry name(s): $($missingEntries -join ', ')."
        }
    }

    if ($entries.Count -eq 0) {
        throw "The GCS content config does not contain any entries to process."
    }

    return $entries
}

function Get-GsUtilPath {
    param(
        [string]$GsUtilPath
    )

    if ($GsUtilPath -and (Test-Path $GsUtilPath)) {
        return (Resolve-Path $GsUtilPath).Path
    }

    $command = Get-Command gsutil -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Unable to find gsutil. Install the Google Cloud SDK and make sure gsutil is on PATH, or pass -GsUtilPath explicitly."
}

function Get-RClonePath {
    param(
        [string]$RClonePath
    )

    if ($RClonePath -and (Test-Path $RClonePath)) {
        return (Resolve-Path $RClonePath).Path
    }

    $command = Get-Command rclone -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Unable to find rclone. Install rclone and make sure it is on PATH, or pass -RClonePath explicitly."
}

function Get-GCloudPath {
    param(
        [string]$GCloudPath
    )

    if ($GCloudPath -and (Test-Path $GCloudPath)) {
        return (Resolve-Path $GCloudPath).Path
    }

    $command = Get-Command gcloud -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Unable to find gcloud. Install the Google Cloud SDK and make sure gcloud is on PATH, or pass -GCloudPath explicitly."
}

function Invoke-GsUtil {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$GsUtilPath
    )

    $resolvedGsUtilPath = Get-GsUtilPath -GsUtilPath $GsUtilPath
    Write-Host ">> $resolvedGsUtilPath $($Arguments -join ' ')"
    & $resolvedGsUtilPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "gsutil failed with exit code $LASTEXITCODE."
    }
}

function Invoke-GCloud {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$GCloudPath
    )

    $resolvedGCloudPath = Get-GCloudPath -GCloudPath $GCloudPath
    Write-Host ">> $resolvedGCloudPath $($Arguments -join ' ')"
    & $resolvedGCloudPath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "gcloud failed with exit code $LASTEXITCODE."
    }
}

function Invoke-RClone {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [string]$RClonePath
    )

    $resolvedRClonePath = Get-RClonePath -RClonePath $RClonePath
    Write-Host ">> $resolvedRClonePath $($Arguments -join ' ')"
    & $resolvedRClonePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "rclone failed with exit code $LASTEXITCODE."
    }
}

function Get-GcsLocalPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [object]$Entry
    )

    return Resolve-RepoPath -Path ([string]$Entry.localPath) -RepoRoot $RepoRoot
}

function Get-GcsEntryKind {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Entry,
        [string]$LocalPath
    )

    $configuredKind = [string]$Entry.kind
    if (-not [string]::IsNullOrWhiteSpace($configuredKind)) {
        $normalizedKind = $configuredKind.Trim().ToLowerInvariant()
        if ($normalizedKind -in @('directory', 'file')) {
            return $normalizedKind
        }

        throw "Unsupported entry kind '$configuredKind' for '$([string]$Entry.name)'. Use 'directory' or 'file'."
    }

    if ($LocalPath -and (Test-Path $LocalPath)) {
        $item = Get-Item -LiteralPath $LocalPath
        if ($item.PSIsContainer) {
            return 'directory'
        }

        return 'file'
    }

    throw "Unable to infer entry kind for '$([string]$Entry.name)'. Add a 'kind' field with 'directory' or 'file' to the config entry."
}

function Get-GcsRemoteUri {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BucketUri,
        [Parameter(Mandatory = $true)]
        [object]$ContentRef,
        [Parameter(Mandatory = $true)]
        [object]$Entry
    )

    $remoteSubpath = ([string]$Entry.remoteSubpath).TrimStart('/').Replace('\\', '/')
    if ([string]$ContentRef.Kind -eq 'snapshot') {
        return "$($BucketUri.TrimEnd('/'))/snapshots/$([string]$ContentRef.Value)/$remoteSubpath"
    }

    return "$($BucketUri.TrimEnd('/'))/$([string]$ContentRef.Value)/$remoteSubpath"
}

function Get-GcsExcludePattern {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Entry
    )

    $regexes = @($Entry.excludeRegexes)
    if ($regexes.Count -eq 0) {
        return $null
    }

    $parts = @()
    foreach ($regex in $regexes) {
        $parts += "(?:$regex)"
    }

    return ($parts -join '|')
}

function Invoke-GcsTransfer {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('push', 'pull')]
        [string]$Direction,
        [Parameter(Mandatory = $true)]
        [ValidateSet('gcs', 'cloudflare')]
        [string]$Vendor,
        [Parameter(Mandatory = $true)]
        [object]$Entry,
        [Parameter(Mandatory = $true)]
        [string]$LocalPath,
        [Parameter(Mandatory = $true)]
        [string]$RemoteUri,
        [string]$GsUtilPath,
        [string]$RClonePath,
        [switch]$DryRun,
        [switch]$SkipDelete
    )

    if ($Vendor -eq 'cloudflare') {
        Invoke-CloudflareTransfer -Direction $Direction -Entry $Entry -LocalPath $LocalPath -RemoteUri $RemoteUri -RClonePath $RClonePath -DryRun:$DryRun -SkipDelete:$SkipDelete
        return
    }

    $entryKind = Get-GcsEntryKind -Entry $Entry -LocalPath $LocalPath
    $excludePattern = Get-GcsExcludePattern -Entry $Entry
    $entryName = [string]$Entry.name

    if ($entryKind -eq 'directory') {
        if ($Direction -eq 'pull' -and -not (Test-Path $LocalPath)) {
            New-Item -ItemType Directory -Path $LocalPath -Force | Out-Null
        }

        if ($Direction -eq 'push' -and -not (Test-Path $LocalPath)) {
            throw "Unable to publish '$entryName' because the local directory '$LocalPath' does not exist."
        }

        $arguments = @('-m', 'rsync', '-r')
        if (-not $SkipDelete) {
            $arguments += '-d'
        }

        if ($DryRun) {
            $arguments += '-n'
        }

        if ($excludePattern) {
            $arguments += @('-x', $excludePattern)
        }

        if ($Direction -eq 'push') {
            $arguments += @($LocalPath, $RemoteUri)
            Write-Host "Publishing directory '$entryName' from '$LocalPath' into '$RemoteUri'."
        }
        else {
            $arguments += @($RemoteUri, $LocalPath)
            Write-Host "Synchronizing directory '$entryName' from '$RemoteUri' into '$LocalPath'."
        }

        Invoke-GsUtil -Arguments $arguments -GsUtilPath $GsUtilPath
        return
    }

    if ($Direction -eq 'pull') {
        $parentDirectory = Split-Path -Parent $LocalPath
        if (-not [string]::IsNullOrWhiteSpace($parentDirectory) -and -not (Test-Path $parentDirectory)) {
            New-Item -ItemType Directory -Path $parentDirectory -Force | Out-Null
        }
    }
    elseif (-not (Test-Path $LocalPath)) {
        throw "Unable to publish '$entryName' because the local file '$LocalPath' does not exist."
    }

    if ($DryRun) {
        if ($Direction -eq 'push') {
            Write-Host "Dry run: would publish file '$entryName' from '$LocalPath' into '$RemoteUri'."
        }
        else {
            Write-Host "Dry run: would pull file '$entryName' from '$RemoteUri' into '$LocalPath'."
        }

        return
    }

    $fileArguments = @('-m', 'cp')
    if ($Direction -eq 'push') {
        $fileArguments += @($LocalPath, $RemoteUri)
        Write-Host "Publishing file '$entryName' from '$LocalPath' into '$RemoteUri'."
    }
    else {
        $fileArguments += @($RemoteUri, $LocalPath)
        Write-Host "Pulling file '$entryName' from '$RemoteUri' into '$LocalPath'."
    }

    Invoke-GsUtil -Arguments $fileArguments -GsUtilPath $GsUtilPath
}

function Invoke-CloudflareTransfer {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet('push', 'pull')]
        [string]$Direction,
        [Parameter(Mandatory = $true)]
        [object]$Entry,
        [Parameter(Mandatory = $true)]
        [string]$LocalPath,
        [Parameter(Mandatory = $true)]
        [string]$RemoteUri,
        [string]$RClonePath,
        [switch]$DryRun,
        [switch]$SkipDelete
    )

    $entryKind = Get-GcsEntryKind -Entry $Entry -LocalPath $LocalPath
    $entryName = [string]$Entry.name

    if ($entryKind -eq 'directory') {
        if ($Direction -eq 'pull' -and -not (Test-Path $LocalPath)) {
            New-Item -ItemType Directory -Path $LocalPath -Force | Out-Null
        }

        if ($Direction -eq 'push' -and -not (Test-Path $LocalPath)) {
            throw "Unable to publish '$entryName' because the local directory '$LocalPath' does not exist."
        }

        $arguments = @('sync')
        if ($DryRun) {
            $arguments += '--dry-run'
        }

        if ($SkipDelete) {
            $arguments += '--ignore-existing'
        }

        if ($Direction -eq 'push') {
            $arguments += @($LocalPath, $RemoteUri)
            Write-Host "Publishing directory '$entryName' from '$LocalPath' into '$RemoteUri' through rclone."
        }
        else {
            $arguments += @($RemoteUri, $LocalPath)
            Write-Host "Synchronizing directory '$entryName' from '$RemoteUri' into '$LocalPath' through rclone."
        }

        Invoke-RClone -Arguments $arguments -RClonePath $RClonePath
        return
    }

    if ($Direction -eq 'pull') {
        $parentDirectory = Split-Path -Parent $LocalPath
        if (-not [string]::IsNullOrWhiteSpace($parentDirectory) -and -not (Test-Path $parentDirectory)) {
            New-Item -ItemType Directory -Path $parentDirectory -Force | Out-Null
        }
    }
    elseif (-not (Test-Path $LocalPath)) {
        throw "Unable to publish '$entryName' because the local file '$LocalPath' does not exist."
    }

    $fileArguments = @('copyto')
    if ($DryRun) {
        $fileArguments += '--dry-run'
    }

    if ($Direction -eq 'push') {
        $fileArguments += @($LocalPath, $RemoteUri)
        Write-Host "Publishing file '$entryName' from '$LocalPath' into '$RemoteUri' through rclone."
    }
    else {
        $fileArguments += @($RemoteUri, $LocalPath)
        Write-Host "Pulling file '$entryName' from '$RemoteUri' into '$LocalPath' through rclone."
    }

    Invoke-RClone -Arguments $fileArguments -RClonePath $RClonePath
}
